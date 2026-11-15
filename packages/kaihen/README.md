# kaihen

The kaiHEN stage is `autoloader-kaihen.mjs` in the CivSwitchFS overlay. It
consumes the FS stage's `fspSrv`/`pm`/`ro` facade and:

1. prepares the SD layout and caches the HEN files;
2. launches the RO donor;
3. boots HEN with the FS root session.

The `kaihenAuto` flag is **true** by default: the autoloader prepares and boots
kaiHEN. Set `devMode` to `true` to keep the bridge alive and boot manually.

## Binaries

Pinned prebuilt files live in `packages/sdcard/vendor/`:

```text
game/main        ounce-hbloader
hen/main         kaiHEN HEN
ro/main          kaiHEN RO
bootstrap.bin    kaiHEN bootstrap
```

Build them from `third_party/kaihen` with the Switch 2 devkit:

```sh
export DEVKITPRO=/path/to/devkitpro
export LIBOUNCE=$DEVKITPRO/libounce
make -C third_party/kaihen
```

Then copy `hen/build/main`, `ro/build/main` and
`bootstrap/build/bootstrap.bin` into `packages/sdcard/vendor/`.

`game/main` and `hbmenu.nro` come from the kaiHEN release
(`ouncebrew/ounce-hbloader`, `ouncebrew/ounce-hbmenu`).
