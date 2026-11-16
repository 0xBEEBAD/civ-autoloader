/*
 * Serial <-> LDN debug bridge.
 *
 * The console keeps CivSwitchFS's elevated window open and listens on UDP
 * :9028.  This task reads lines from the ESP32 USB-Serial/JTAG console, sends
 * them as C<seq>:<line> to the console, and prints B<seq>:<result> replies.
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "driver/usb_serial_jtag.h"

#include "civ7.h"
#include "bridge.h"
#include "installer.h"

static const char *TAG = "bridge";
#define BRIDGE_PORT 9028
#define BRIDGE_LINE_MAX 1024

static uint8_t s_peer[4];
static volatile bool s_have_peer;
static volatile bool s_nhole_request;
static uint32_t s_seq;

static void send_line(const char *line) {
    if (!s_have_peer) {
        ESP_LOGW(TAG, "no console peer yet (waiting for R)");
        return;
    }
    char packet[BRIDGE_LINE_MAX + 32];
    int n = snprintf(packet, sizeof(packet), "C%u:%s", (unsigned)++s_seq, line);
    if (n <= 0 || n >= (int)sizeof(packet)) return;
    int r = civ7_udp_send(BRIDGE_PORT, (const uint8_t *)packet, (size_t)n);
    if (r != 0) ESP_LOGW(TAG, "udp send failed: %d", r);
}

static void bridge_task(void *arg) {
    (void)arg;
    char line[BRIDGE_LINE_MAX];
    size_t used = 0;
    for (;;) {
        uint8_t ch;
        int n = usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(100));
        if (n <= 0) continue;
        if (ch == '\r') continue;
        if (ch == '\n') {
            line[used] = 0;
            if (used && strncmp(line, "stages ", 7) == 0) installer_set_stages(line + 7);
            else if (used && strcmp(line, "sendjs") == 0) installer_loaderjs_run();
            else if (used && strcmp(line, "nhole") == 0) s_nhole_request = true;
            else if (used) send_line(line);
            used = 0;
        } else if (used + 1 < sizeof(line)) {
            line[used++] = (char)ch;
        }
    }
}

void bridge_start(void) {
    usb_serial_jtag_driver_config_t cfg = {
        .rx_buffer_size = 1024,
        .tx_buffer_size = 1024,
    };
    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "usb serial driver install failed: %s",
                 esp_err_to_name(err));
        return;
    }
    if (xTaskCreate(bridge_task, "bridge", 4096, NULL, 3, NULL) != pdPASS)
        ESP_LOGE(TAG, "task create failed");
}

void bridge_udp_rx(const uint8_t *data, size_t len, const uint8_t src_ip[4]) {    if (!data || len == 0) return;
    if (data[0] == 'R' && len == 1) {
        memcpy(s_peer, src_ip, 4);
        s_have_peer = true;
        ESP_LOGI(TAG, "console ready at %u.%u.%u.%u",
                 src_ip[0], src_ip[1], src_ip[2], src_ip[3]);
        return;
    }
    if (data[0] == 'B' && len > 1) {
        char line[BRIDGE_LINE_MAX];
        size_t n = len - 1;
        if (n > sizeof(line) - 1) n = sizeof(line) - 1;
        memcpy(line, data + 1, n);
        line[n] = 0;
        ESP_LOGI(TAG, "reply %s", line);
        return;
    }
}

/* Set by the serial command "nhole"; the main loop consumes it. */
bool bridge_take_nhole_request(void) {
    if (!s_nhole_request) return false;
    s_nhole_request = false;
    return true;
}
