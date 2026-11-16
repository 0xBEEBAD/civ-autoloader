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

`hbmenu.nro` is from the **kaiHEN 20261116** release: ounce-hbmenu
`43845d2`, which no longer looks up the system language at startup. The
previous build (`d7e10c0`) called `setMakeLanguage`, which returns
2345-0011 on consoles whose language is not in the settings service's list
for applications, and the menu then did not start.

If you replace the file yourself, take it from a kaiHEN release, not from a
raw ounce-hbmenu checkout: the Switch 2 build needs portlibs that the plain
devkitPro image does not ship.

OunceShell is intentionally not included.
