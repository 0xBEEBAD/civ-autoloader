/*
 * N-Hole: SoftAP + Nintendo-aware DNS sinkhole + mini HTTP responder.
 *
 * After the LDN install finishes, the board stops using its radio for LDN and
 * becomes a small access point called "N-Hole". A client (the console, a phone)
 * joins it and uses the ESP32 as its DNS server:
 *
 *   ctest.* / conntest.*   -> the AP's own address; :80 answers 200 OK
 *   other Nintendo names   -> 0.0.0.0 / ::
 *   everything else        -> NXDOMAIN
 *
 * The served names and the Nintendo suffixes follow the nanodns kaiHEN plugin
 * (nanodns/source/config.c). nanodns forwards every other name to an upstream
 * resolver because it runs on the console and the console keeps its internet.
 * The board is AP-only: there is no station uplink, so forwarding would just
 * stall on an unreachable resolver and then time out. Other names get NXDOMAIN
 * immediately instead. If a future build brings up a station uplink, the
 * forwarding path below is what nanodns does today.
 */
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

#include "nhole.h"

static const char *TAG = "nhole";

#define DNS_PORT 53
#define HTTP_PORT 80
#define DNS_PACKET_MAX 512
#define NHOLE_NAME_MAX 128   /* a DNS name never reaches this; the buffer below is 2x */
#define DNS_POLL_MS 500

static const char *const kServeNames[] = {
    "ctest.cdn.nintendo.net",
    "ctest.srv.nintendo.net",
    "conntest.nintendowifi.net",
    NULL
};

static const char *const kNintendoSuffixes[] = {
    "nintendo.net", "nintendo.com", "nintendowifi.net",
    "nintendoswitch.cn", "nintendo.co.jp", "nintendo.co.uk",
    "nintendo-europe.com", "nintendo.com.au", "nintendo.co.kr",
    "nintendo.com.hk", "nintendo.com.tw", "nintendo.cn",
    NULL
};

static NholeCountersSnapshot s_counters;
static uint32_t s_answer_ip;        /* AP address, network order */
static esp_netif_t *s_ap_netif;
static httpd_handle_t s_http;
static volatile bool s_running;

/* --------------------------------------------------------------- helpers */

static bool name_matches(const char *name, const char *entry) {
    if (strcmp(name, entry) == 0) return true;
    size_t n = strlen(name), m = strlen(entry);
    if (n <= m) return false;
    const char *tail = name + (n - m);
    return tail[-1] == '.' && strcmp(tail, entry) == 0;
}

static bool is_served(const char *name) {
    for (int i = 0; kServeNames[i]; i++)
        if (name_matches(name, kServeNames[i])) return true;
    return false;
}

static bool is_nintendo(const char *name) {
    for (int i = 0; kNintendoSuffixes[i]; i++) {
        const char *suffix = kNintendoSuffixes[i];
        size_t n = strlen(name), m = strlen(suffix);
        if (n < m) continue;
        const char *tail = name + (n - m);
        if (strcmp(tail, suffix) != 0) continue;
        if (n == m) return true;
        if (tail[-1] == '.') return true;
    }
    return false;
}

/* ------------------------------------------------------------ DNS server */

enum {
    DNS_MODE_NXDOMAIN = 0,
    DNS_MODE_A = 1,
    DNS_MODE_AAAA = 2,
    DNS_MODE_NODATA = 3,
    DNS_MODE_A_ZERO = 4,
};

static int read_name(const uint8_t *packet, int length, int offset, char *out,
                     int capacity, int *end) {
    int position = offset, end_position = -1, jumps = 0, used = 0;
    if (!end || capacity < 2) return -1;
    while (position < length && jumps < 16) {
        uint8_t c = packet[position];
        if ((c & 0xc0) == 0xc0) {
            if (position + 1 >= length) return -1;
            if (end_position < 0) end_position = position + 2;
            position = ((c & 0x3f) << 8) | packet[position + 1];
            jumps++;
            continue;
        }
        if (c == 0) {
            if (end_position < 0) end_position = position + 1;
            break;
        }
        if ((c & 0xc0) != 0 || position + 1 + c > length) return -1;
        if (used && used < capacity - 1) out[used++] = '.';
        for (int i = 0; i < c && used < capacity - 1; i++)
            out[used++] = (char)tolower(packet[position + 1 + i]);
        position += 1 + c;
    }
    if (end_position < 0) return -1;
    out[used] = 0;
    *end = end_position;
    return 0;
}

static int put_question(uint8_t *out, int at, const char *name, uint16_t qtype,
                        uint16_t qclass) {
    const char *p = name;
    while (*p) {
        const char *dot = strchr(p, '.');
        int n = dot ? (int)(dot - p) : (int)strlen(p);
        if (n <= 0 || n > 63) return -1;
        out[at++] = (uint8_t)n;
        memcpy(out + at, p, (size_t)n);
        at += n;
        p = dot ? dot + 1 : p + n;
    }
    out[at++] = 0;
    out[at++] = (uint8_t)(qtype >> 8);  out[at++] = (uint8_t)qtype;
    out[at++] = (uint8_t)(qclass >> 8); out[at++] = (uint8_t)qclass;
    return at;
}

static int build_response(const uint8_t *query, const char *name, uint16_t qtype,
                          uint16_t qclass, int mode, uint32_t answer_ip,
                          uint8_t *out, int out_capacity) {
    int at = 12;
    if ((at = put_question(out, at, name, qtype, qclass)) < 0) return -1;

    out[0] = query[0]; out[1] = query[1];
    out[2] = 0x81;
    out[3] = mode == DNS_MODE_NXDOMAIN ? 0x83 : 0x80;
    out[4] = 0x00; out[5] = 0x01;
    out[6] = 0x00; out[7] = 0x00;
    out[8] = 0x00; out[9] = 0x00;
    out[10] = 0x00; out[11] = 0x00;

    if (mode == DNS_MODE_NXDOMAIN || mode == DNS_MODE_NODATA) return at;
    if (at + 2 + 2 + 2 + 4 + 2 + 16 > out_capacity) return -1;

    out[6] = 0x00; out[7] = 0x01;
    out[at++] = 0xc0; out[at++] = 0x0c;
    out[at++] = (uint8_t)(qtype >> 8);  out[at++] = (uint8_t)qtype;
    out[at++] = (uint8_t)(qclass >> 8); out[at++] = (uint8_t)qclass;
    out[at++] = 0; out[at++] = 0; out[at++] = 0; out[at++] = 60;
    if (mode == DNS_MODE_A || mode == DNS_MODE_A_ZERO) {
        out[at++] = 0x00; out[at++] = 0x04;
        uint32_t ip = mode == DNS_MODE_A ? answer_ip : 0;
        memcpy(out + at, &ip, 4);
        at += 4;
    } else {
        out[at++] = 0x00; out[at++] = 0x10;
        memset(out + at, 0, 16);
        at += 16;
    }
    return at;
}

static void handle_query(const uint8_t *query, int query_length, uint8_t *out,
                         struct sockaddr_in *from, socklen_t from_length, int fd) {
    char name[NHOLE_NAME_MAX * 2];
    int question_end = 0;
    if (read_name(query, query_length, 12, name, sizeof(name), &question_end) != 0)
        return;
    if (question_end + 4 > query_length) return;

    uint16_t qtype = ((uint16_t)query[question_end] << 8) | query[question_end + 1];
    uint16_t qclass = ((uint16_t)query[question_end + 2] << 8) | query[question_end + 3];
    s_counters.queries++;

    int mode;
    if (is_served(name)) {
        s_counters.served++;
        mode = qtype == 1 ? DNS_MODE_A : DNS_MODE_NODATA;
    } else if (is_nintendo(name)) {
        s_counters.blocked++;
        if (qtype == 1) mode = DNS_MODE_A_ZERO;
        else if (qtype == 28) mode = DNS_MODE_AAAA;
        else mode = DNS_MODE_NODATA;
    } else {
        /* No uplink in AP mode: answer immediately instead of stalling on a
         * resolver that cannot be reached. */
        s_counters.nxdomain++;
        mode = DNS_MODE_NXDOMAIN;
    }

    int length = build_response(query, name, qtype, qclass, mode, s_answer_ip,
                                out, DNS_PACKET_MAX);
    if (length > 0)
        sendto(fd, out, (size_t)length, 0, (struct sockaddr *)from, from_length);
}

static void dns_task(void *argument) {
    (void)argument;
    uint8_t query[DNS_PACKET_MAX];
    uint8_t response[DNS_PACKET_MAX];

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { ESP_LOGE(TAG, "dns socket failed"); vTaskDelete(NULL); return; }
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct timeval timeout = {.tv_sec = 0, .tv_usec = DNS_POLL_MS * 1000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(DNS_PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        ESP_LOGE(TAG, "dns bind :53 failed errno=%d", errno);
        close(fd);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "DNS listening on :53");

    while (s_running) {
        struct sockaddr_in from;
        socklen_t from_length = sizeof(from);
        int received = recvfrom(fd, query, sizeof(query), 0,
                                (struct sockaddr *)&from, &from_length);
        if (received < 12) continue;   /* timeout or runt: re-check the flag */
        handle_query(query, received, response, &from, from_length, fd);
    }
    close(fd);
    vTaskDelete(NULL);
}

/* ----------------------------------------------------------- HTTP server */

static bool host_is_ctest(const char *host) {
    for (int i = 0; kServeNames[i]; i++)
        if (name_matches(host, kServeNames[i])) return true;
    return false;
}

/* nanodns routes on the Host header alone: any path, any method. */
static esp_err_t http_handler(httpd_req_t *request) {
    char host[NHOLE_NAME_MAX] = "";
    if (httpd_req_get_hdr_value_str(request, "Host", host, sizeof(host)) == ESP_OK) {
        char *colon = strchr(host, ':');
        if (colon) *colon = 0;
        for (char *p = host; *p; p++) *p = (char)tolower((unsigned char)*p);
    }
    if (!host_is_ctest(host)) {
        s_counters.http_404++;
        ESP_LOGD(TAG, "404 %s %s", host, request->uri);
        return httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "not found");
    }
    /* Drain a request body so the client does not see a reset. */
    if (request->content_len > 0) {
        char drain[64];
        int left = request->content_len;
        while (left > 0) {
            int n = httpd_req_recv(request, drain,
                                   left > (int)sizeof(drain) ? (int)sizeof(drain) : left);
            if (n <= 0) break;
            left -= n;
        }
    }
    s_counters.http_ok++;
    httpd_resp_set_status(request, "200 OK");
    httpd_resp_set_hdr(request, "X-Organization", "Nintendo");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_type(request, "text/plain");
    if (request->method == HTTP_HEAD) return httpd_resp_send(request, NULL, 0);
    return httpd_resp_send(request, "ok", 2);
}

static const httpd_uri_t s_catch_all = {
    .uri = "/*", .method = HTTP_ANY, .handler = http_handler,
};

static void http_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_PORT;
    config.lru_purge_enable = true;
    /* Wildcard matching, so the single catch-all handler covers every path. */
    config.uri_match_fn = httpd_uri_match_wildcard;
    if (httpd_start(&s_http, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        s_http = NULL;
        return;
    }
    esp_err_t err = httpd_register_uri_handler(s_http, &s_catch_all);
    if (err != ESP_OK) ESP_LOGE(TAG, "httpd register failed: %s", esp_err_to_name(err));
    ESP_LOGI(TAG, "HTTP listening on :80");
}

static void http_stop(void) {
    if (s_http) {
        httpd_stop(s_http);
        s_http = NULL;
    }
}

/* -------------------------------------------------------------- SoftAP */

/* Waits for the AP interface to be assigned its address. */
static uint32_t wait_for_ap_address(void) {
    for (int i = 0; i < 40; i++) {
        esp_netif_ip_info_t info;
        if (esp_netif_get_ip_info(s_ap_netif, &info) == ESP_OK && info.ip.addr) {
            s_answer_ip = info.ip.addr;
            return info.ip.addr;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return 0;
}

void nhole_counters(NholeCountersSnapshot *out) {
    if (out) *out = s_counters;
}

esp_err_t nhole_start(void) {
    if (s_running) return ESP_OK;
    memset(&s_counters, 0, sizeof(s_counters));

    /* Bring the radio down cleanly: the LDN path left it in station mode with
     * promiscuous capture and a patched wpa table. */
    esp_wifi_set_promiscuous(false);
    esp_wifi_disconnect();
    esp_wifi_stop();

    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (!s_ap_netif) { ESP_LOGE(TAG, "AP netif failed"); return ESP_FAIL; }

    wifi_config_t config = {0};
    snprintf((char *)config.ap.ssid, sizeof(config.ap.ssid), "%s", NHOLE_SSID);
    config.ap.ssid_len = strlen(NHOLE_SSID);
    config.ap.channel = NHOLE_CHANNEL;
    config.ap.max_connection = 4;
    config.ap.authmode = WIFI_AUTH_OPEN;
    config.ap.pmf_cfg.required = false;

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &config);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AP start failed: %s", esp_err_to_name(err));
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
        return err;
    }

    if (!wait_for_ap_address()) {
        ESP_LOGE(TAG, "AP has no address");
        nhole_stop();
        return ESP_FAIL;
    }

    s_running = true;
    struct in_addr answer = {.s_addr = s_answer_ip};
    ESP_LOGI(TAG, "N-Hole up: ssid=%s ip=%s dns=%s",
             NHOLE_SSID, inet_ntoa(answer), inet_ntoa(answer));

    if (xTaskCreate(dns_task, "nhole-dns", 4096, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "dns task create failed");
        nhole_stop();
        return ESP_FAIL;
    }
    http_start();
    return ESP_OK;
}

void nhole_stop(void) {
    if (!s_running && !s_ap_netif) return;
    s_running = false;
    http_stop();
    /* The DNS task wakes on its receive timeout and exits; give it a cycle. */
    vTaskDelay(pdMS_TO_TICKS(DNS_POLL_MS + 200));
    esp_wifi_stop();
    if (s_ap_netif) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }
    ESP_LOGI(TAG, "N-Hole down: q=%u served=%u blocked=%u nx=%u http=%u/%u",
             (unsigned)s_counters.queries, (unsigned)s_counters.served,
             (unsigned)s_counters.blocked, (unsigned)s_counters.nxdomain,
             (unsigned)s_counters.http_ok, (unsigned)s_counters.http_404);
}

bool nhole_running(void) {
    return s_running;
}
