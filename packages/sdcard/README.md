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

## hbmenu version

`hbmenu.nro` is ounce-hbmenu **d7e10c0**. That build looks up the system
language at startup through `setMakeLanguage`, which returns 2345-0011 on
consoles whose language is not in the settings service's list; the menu then
does not start.

Upstream kaiHEN moved to **43845d2**, which drops the lookup. Replace the
vendor file when a Switch 2 build of that commit is available; the rest of the
chain is unchanged.

OunceShell is intentionally not included.
