# fs

The FS stage is the CivSwitchFS fork plus the autoloader overlay:

```text
third_party/CivSwitchFS/
  src/core.js                  hook: onElevated -> runAutoloaderStage
  src/core/autoloader.mjs      orchestrator (FS -> kaiHEN)
  src/core/autoloader-fs.mjs   FS stage
  src/core/autoloader-kaihen.mjs kaiHEN stage
  src/core/config.mjs          skipPidPromotion
  src/core/promotion.mjs       onElevated hook, closeUserStorage
  src/core/visuals.mjs         console log tail / bottom padding
```

The firmware build installs the persistent autoloader save:

```text
blobs/blob_tpl.bin    save template
blobs/blob_apply.bin  save installer + compressed CFS stage
```

The save runs CFS on the next `Load Game`. The same-session CFS stage is not
used because it can freeze the console. If the persistent save causes FS
canary failures, the standard save is a fallback: use
`make_install_blobs.py` and the `save` stage.

Tested on Switch 2 firmware 23.0.1.
