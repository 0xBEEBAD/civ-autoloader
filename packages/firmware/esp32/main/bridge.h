#pragma once
#include <stdint.h>
#include <stdbool.h>

#include <stddef.h>

void bridge_start(void);
void bridge_send_command(const char *line);
void bridge_udp_rx(const uint8_t *data, size_t len, const uint8_t src_ip[4]);
/// True while the serial command "ldn" is pending.
bool bridge_button_pending(void);
/// Consumes the serial command "ldn".
bool bridge_take_button_request(void);
