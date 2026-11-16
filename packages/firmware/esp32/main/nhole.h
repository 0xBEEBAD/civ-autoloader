/*
 * N-Hole: SoftAP + Nintendo-aware DNS sinkhole + mini HTTP responder.
 *
 * After the LDN install, the board drops the LDN link and becomes an access
 * point. See nhole.c for the policy.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define NHOLE_SSID "N-Hole"
#define NHOLE_CHANNEL 6

typedef struct {
    uint32_t queries;
    uint32_t served;
    uint32_t blocked;
    uint32_t nxdomain;
    uint32_t http_ok;
    uint32_t http_404;
} NholeCountersSnapshot;

/// Brings up the access point, the DNS sinkhole and the HTTP responder.
esp_err_t nhole_start(void);

/// Stops HTTP, DNS and the access point.
void nhole_stop(void);

/// True while the access point is up.
bool nhole_running(void);

/// Copies the counters.
void nhole_counters(NholeCountersSnapshot *out);
