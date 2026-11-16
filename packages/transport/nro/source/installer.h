#ifndef CIVSWITCH_INSTALLER_H
#define CIVSWITCH_INSTALLER_H

#include <switch.h>

typedef struct {
    const char *message;
    u32 error_code;
    u32 console_ip;
    unsigned step;
    unsigned total;
    bool completed;
} InstallerResult;

typedef void (*InstallerProgress)(const char *message, unsigned step,
                                  unsigned total);

/// Send the persistent autoloader save over the LDN link, exactly like the
/// ESP32 firmware: bind the LDN local address on :9027, send the A header,
/// stream the template and installer chunks, repair NAKs, and wait for the
/// console's I:OK confirmation. The caller must keep LDN connected.
InstallerResult installer_run_over_ldn(LdnIpv4Address host,
                                       InstallerProgress on_progress);

#endif
