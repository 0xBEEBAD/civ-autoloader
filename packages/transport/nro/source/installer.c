// Save installer over LDN, matching the ESP32 firmware's UDP :9027 protocol.
//
// Board -> console: A<tpl>:<js>:0:0:<local ip>, T<i>:<base64>, J<i>:<base64>, G
// Console -> board: D (all chunks arrived), t<i>/j<i> (resend), I:OK / I:ERR
//
// The console evaluates the payload with its UDP receiver bound on :9027, so
// everything here stays on the LDN link: no Wi-Fi, no prod.keys, no TCP.

#include "installer.h"
#include "installer_payload.h"
#include "save_template_bin.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define INSTALLER_PORT 9027
#define CHUNK 1100              // base64 chars per datagram (matches ESP32)
#define REPORT_LIMIT 96         // missing chunks requested per repair round
#define SEND_DELAY_NS 1000000ULL
#define REPAIR_ROUNDS 90
#define RECEIVE_WINDOW_NS 150000000ULL
#define INSTALL_TIMEOUT_NS 300000000000ULL

static const char base64_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static InstallerResult fail(const char *message, int error, unsigned step,
                            unsigned total) {
    return (InstallerResult){message, (u32)error, 0, step, total, false};
}

static size_t encode_base64(char *out, const u8 *data, size_t size) {
    size_t read = 0, written = 0;
    while (read + 2 < size) {
        u32 value = ((u32)data[read] << 16) | ((u32)data[read + 1] << 8) |
                    (u32)data[read + 2];
        out[written++] = base64_alphabet[(value >> 18) & 63];
        out[written++] = base64_alphabet[(value >> 12) & 63];
        out[written++] = base64_alphabet[(value >> 6) & 63];
        out[written++] = base64_alphabet[value & 63];
        read += 3;
    }
    if (read < size) {
        u32 value = (u32)data[read] << 16;
        out[written++] = base64_alphabet[(value >> 18) & 63];
        if (read + 1 < size) {
            value |= (u32)data[read + 1] << 8;
            out[written++] = base64_alphabet[(value >> 12) & 63];
            out[written++] = base64_alphabet[(value >> 6) & 63];
            out[written++] = '=';
        } else {
            out[written++] = base64_alphabet[(value >> 12) & 63];
            out[written++] = '=';
            out[written++] = '=';
        }
    }
    return written;
}

typedef struct {
    int fd;
    struct sockaddr_in target;
    char *template_b64;
    char *apply_b64;
    unsigned template_chunks;
    unsigned apply_chunks;
} Link;

static int send_datagram(Link *link, const void *data, size_t size) {
    ssize_t sent = sendto(link->fd, data, size, 0,
                          (const struct sockaddr *)&link->target,
                          sizeof(link->target));
    return sent == (ssize_t)size ? 0 : -1;
}

// Send one chunk as "<kind><index>:<base64>". Returns 0 on success.
static int send_chunk(Link *link, char kind, unsigned index) {
    const char *body = kind == 'T' ? link->template_b64 : link->apply_b64;
    unsigned count = kind == 'T' ? link->template_chunks : link->apply_chunks;
    if (index >= count) return -1;

    static char packet[CHUNK + 32];
    int prefix = snprintf(packet, sizeof(packet), "%c%u:", kind, index);
    if (prefix <= 0 || (size_t)prefix >= sizeof(packet)) return -1;

    size_t offset = (size_t)index * CHUNK;
    size_t length = strlen(body + offset);
    if (length > CHUNK) length = CHUNK;
    memcpy(packet + prefix, body + offset, length);
    return send_datagram(link, packet, (size_t)prefix + length);
}

InstallerResult installer_run_over_ldn(LdnIpv4Address host,
                                       InstallerProgress on_progress) {
    Link link = {0};
    InstallerResult result = fail("Installer did not start", 0, 0, 0);

    Result rc = socketInitializeDefault();
    if (R_FAILED(rc))
        return fail("socketInitialize failed", (int)rc, 0, 0);

    link.fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (link.fd < 0) {
        result = fail("socket() failed", errno, 0, 0);
        goto finish_socket;
    }

    LdnIpv4Address local;
    LdnSubnetMask mask;
    rc = ldnGetIpv4Address(&local, &mask);
    if (R_FAILED(rc)) {
        result = fail("ldnGetIpv4Address failed", (int)rc, 0, 0);
        goto finish;
    }

    struct sockaddr_in source = {0};
    source.sin_family = AF_INET;
    source.sin_port = htons(INSTALLER_PORT);
    source.sin_addr.s_addr = htonl(local.addr);
    if (bind(link.fd, (const struct sockaddr *)&source, sizeof(source)) < 0) {
        result = fail("bind(:9027) failed", errno, 0, 0);
        goto finish;
    }

    link.target.sin_family = AF_INET;
    link.target.sin_port = htons(INSTALLER_PORT);
    link.target.sin_addr.s_addr = htonl(host.addr);

    struct timeval timeout = {.tv_sec = 0, .tv_usec = 50000};
    setsockopt(link.fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // Encode both payloads once.
    size_t template_b64_size = ((save_template_bin_size + 2) / 3) * 4;
    size_t apply_b64_size = ((sizeof(installer_apply_js) - 1 + 2) / 3) * 4;
    link.template_b64 = malloc(template_b64_size + 8);
    link.apply_b64 = malloc(apply_b64_size + 8);
    if (!link.template_b64 || !link.apply_b64) {
        result = fail("Not enough memory for the installer", ENOMEM, 0, 0);
        goto finish;
    }
    size_t encoded = encode_base64(link.template_b64, save_template_bin,
                                   save_template_bin_size);
    if (encoded != installer_template_base64_length) {
        result = fail("Template length mismatch", EINVAL, 0, 0);
        goto finish;
    }
    link.template_b64[encoded] = 0;
    encoded = encode_base64(link.apply_b64,
                            (const u8 *)installer_apply_js,
                            sizeof(installer_apply_js) - 1);
    link.apply_b64[encoded] = 0;

    link.template_chunks = (unsigned)((encoded = strlen(link.template_b64)) +
                                      CHUNK - 1) / CHUNK;
    link.apply_chunks = (unsigned)((strlen(link.apply_b64) + CHUNK - 1) / CHUNK);

    // Header: our 5-field form with cfs/start set to 0.
    char header[96];
    int header_size = snprintf(header, sizeof(header), "A%u:%u:0:0:%u.%u.%u.%u",
                               link.template_chunks, link.apply_chunks,
                               (unsigned)((local.addr >> 24) & 0xFF),
                               (unsigned)((local.addr >> 16) & 0xFF),
                               (unsigned)((local.addr >> 8) & 0xFF),
                               (unsigned)(local.addr & 0xFF));
    if (header_size <= 0) {
        result = fail("Header build failed", EINVAL, 0, 0);
        goto finish;
    }
    if (on_progress) on_progress("Sending save over LDN", 0,
                                 link.template_chunks + link.apply_chunks);
    for (int attempt = 0; attempt < 3; attempt++) {
        if (send_datagram(&link, header, (size_t)header_size) < 0) {
            result = fail("Header send failed", errno, 0, 0);
            goto finish;
        }
        svcSleepThread(100000000ULL);
    }

    unsigned total = link.template_chunks + link.apply_chunks;
    unsigned sent = 0;
    for (unsigned i = 0; i < link.apply_chunks; i++, sent++) {
        if (send_chunk(&link, 'J', i) < 0) {
            result = fail("Installer chunk send failed", errno, sent, total);
            goto finish;
        }
        svcSleepThread(SEND_DELAY_NS);
    }
    for (unsigned i = 0; i < link.template_chunks; i++, sent++) {
        if (send_chunk(&link, 'T', i) < 0) {
            result = fail("Template chunk send failed", errno, sent, total);
            goto finish;
        }
        if (on_progress && (i % 64) == 0)
            on_progress("Sending save over LDN", sent, total);
        svcSleepThread(SEND_DELAY_NS);
    }

    if (on_progress) on_progress("Repairing missing chunks", sent, total);

    u64 deadline = armGetSystemTick() + armNsToTicks(INSTALL_TIMEOUT_NS);
    bool transfer_complete = false;
    bool install_ok = false;
    bool install_failed = false;
    int rounds = 0;
    char packet[2048];

    while (armGetSystemTick() < deadline && appletMainLoop()) {
        if (!transfer_complete) {
            if (rounds >= REPAIR_ROUNDS) break;
            if (send_datagram(&link, "G", 1) < 0) break;
            rounds++;
        }

        ssize_t size = recv(link.fd, packet, sizeof(packet) - 1, 0);
        if (size <= 0) continue;
        packet[size] = 0;

        if (size == 1 && packet[0] == 'D') {
            transfer_complete = true;
            if (on_progress) on_progress("Console received the save; installing", total, total);
            continue;
        }
        if (strncmp(packet, "I:OK", 4) == 0) {
            install_ok = true;
            break;
        }
        if (strncmp(packet, "I:ERR", 5) == 0) {
            install_failed = true;
            break;
        }
        if (packet[0] == 't' || packet[0] == 'j') {
            char kind = packet[0] == 't' ? 'T' : 'J';
            unsigned index = (unsigned)strtoul(packet + 1, NULL, 10);
            send_chunk(&link, kind, index);
            svcSleepThread(SEND_DELAY_NS);
        }
    }

    if (install_ok) {
        result = (InstallerResult){"Persistent trigger save installed", 0,
                                   0, total, total, true};
    } else if (install_failed) {
        result = fail("The console rejected the save installer", EPROTO,
                      total, total);
    } else if (!transfer_complete) {
        result = fail("Timed out repairing installer chunks", ETIMEDOUT,
                      total, total);
    } else {
        result = fail("Timed out waiting for the console confirmation",
                      ETIMEDOUT, total, total);
    }

finish:
    free(link.template_b64);
    free(link.apply_b64);
    if (link.fd >= 0) close(link.fd);
finish_socket:
    socketExit();
    return result;
}
