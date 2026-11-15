# sdcard

```text
vendor/                 pinned prebuilt files (tracked)
  hbmenu.nro            ounce-hbmenu
  game/main             ounce-hbloader
  hen/main              kaiHEN HEN
  ro/main               kaiHEN RO
  bootstrap.bin         kaiHEN bootstrap
  hen-options.txt       kaiHEN options
  plugins/ftpd.nso      optional FTP plugin
licenses/               third-party license texts
contents/               generated staging (gitignored)
```

Stage and copy:

```sh
python3 packages/sdcard/stage.py
cp -R packages/sdcard/contents/* /Volumes/<SD>/
```

The resulting SD layout:

```text
/hbmenu.nro
/kaihen/bootstrap.bin
/kaihen/bin/game/main
/kaihen/bin/hen/main
/kaihen/bin/ro/main
/kaihen/hen-options.txt
/kaihen/plugins/ftpd.nso   (optional)
/licenses/...
```

OunceShell is intentionally not included.
