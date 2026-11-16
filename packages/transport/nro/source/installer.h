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

/// Which save the console writes.
typedef enum {
    /// CivSwitchFS autoloader save: Load Game runs FS elevation, prepares and
    /// boots kaiHEN (the R path).
    InstallerKind_Autoloader = 0,
    /// Standard CivSwitch save: Load Game brings the Remote JS Loader back up
    /// on TCP :9026 (the ZR path).
    InstallerKind_Classic = 1,
} InstallerKind;

/// Send a save over the LDN link, exactly like the ESP32 firmware: bind the
/// LDN local address on :9027, announce the stage, send the A header, stream
/// the template and installer chunks, repair NAKs, and wait for the console's
/// I:OK confirmation. The caller must keep LDN connected.
InstallerResult installer_run_over_ldn(LdnIpv4Address host, InstallerKind kind,
                                       InstallerProgress on_progress);

#endif
