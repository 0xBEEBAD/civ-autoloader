#pragma once
#include <stdint.h>
#include <stdbool.h>

#include <stddef.h>

void bridge_start(void);
void bridge_send_command(const char *line);
void bridge_udp_rx(const uint8_t *data, size_t len, const uint8_t src_ip[4]);
/// True once after the serial command "nhole" was typed.
bool bridge_take_nhole_request(void);
