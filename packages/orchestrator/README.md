# orchestrator

The orchestrator is currently `third_party/CivSwitchFS/src/core/autoloader.mjs`.
It runs the stages in order:

```text
runFs(input)                  # CivSwitchFS elevation
createKaihen(fs)              # kaiHEN facade
kaihen.prepare()              # SD layout + RO donor + cached HEN files
if kaihenAuto && !dev: boot() # one-click path
fs.keepAlive(...)             # bridge / manual commands
```

`kaihenAuto` is **true** by default. `devMode` (or
`globalThis.__CIVSWITCH_DEV__`) keeps the manual bridge. The firmware build
stamps `BUILD_UTC` into this file and restores it afterwards.

The next refactor is to move this file to `packages/orchestrator/` and have the
CFS build import it, so the FS fork stays clean. Until then, the file lives in
the CFS checkout and is listed in `versions.json`.
