# userland

The game-side userland loader is the CivSwitch fork:

```text
third_party/CivSwitch/core.js
```

The ESP32 firmware embeds it at build time:

```text
core.js + tools/udp_loader.js
  -> PicoSwitch/compress.js (MAX=1000)
  -> packages/firmware/esp32/main/chat_payload.h
```

There are no local changes to `core.js` in this repository. The LDN transport
and the UDP installer live in the firmware package, not in the upstream
userland loader.
