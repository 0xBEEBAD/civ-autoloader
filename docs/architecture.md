# Architecture

## Layers

```text
ESP32 LDN transport
  -> CivSwitch userland loader (third_party/CivSwitch/core.js)
  -> CivSwitchFS elevation (third_party/CivSwitchFS + overlay)
  -> kaiHEN installer/boot (autoloader-kaihen.mjs)
  -> hbmenu
```

The ESP32 is only a transport. It joins the game's local-wireless lobby,
delivers the userland payload, and streams the CFS blob. It does not contain
FS or kaiHEN logic.

## Modules

### userland

`third_party/CivSwitch/core.js` provides the game-side exploit and the loader
API:

```text
sdk_base, main_base, read/write helpers, call_rop, malloc/free, log
```

It does not know about FS or kaiHEN.

### fs

`third_party/CivSwitchFS` elevates `fsp-srv` and exposes the elevated session.
The autoloader overlay lives in its `src/core/`:

```text
autoloader.mjs        orchestrator
autoloader-fs.mjs     FS stage
autoloader-kaihen.mjs kaiHEN stage
```

The FS stage exposes `fspSrv`, `pm`, `ro`, and the promotion context. It does
not import kaiHEN.

### kaihen

`autoloader-kaihen.mjs` consumes the FS facade:

```text
prepare()  SD layout, cached HEN files, RO donor
boot()     launch RO, start HEN
```

`kaihenAuto` is true by default. `devMode` keeps the bridge alive and makes
boot a manual step.

### orchestrator

`autoloader.mjs` runs the stages in order:

```text
runFs(input)
createKaihen(fs)
kaihen.prepare()
if kaihenAuto && !dev: kaihen.boot()
fs.keepAlive(kaihen.commands())
```

### firmware

`packages/firmware/esp32` is the ESP-IDF project. It embeds:

```text
chat_payload.h   userland + UDP installer
blob_tpl.bin     original save template
blob_apply.bin   save installer
blob_cfs.bin     CivSwitchFS + overlay
blob_loaderjs.bin loader JS (optional)
```

### sdcard

`packages/sdcard/vendor` holds the pinned prebuilt binaries.
`packages/sdcard/stage.py` builds `contents/`, which is copied to the SD card.

## Stage manifest

Switch 2 (current):

```text
persist (save installer) -> CFS -> prepare kaiHEN -> boot kaiHEN
```

The persistent save contains the CFS stage, so the ESP32 does not stream a
separate CFS blob.
