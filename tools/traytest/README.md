traytest
========

Small XEmbed systray client for testing `dwtoolset`.

Start `dwtoolset` first, then:

```sh
make
./traytest -n 10
```

It creates ten override-redirect icon windows of different sizes and colors,
sends `SYSTEM_TRAY_REQUEST_DOCK` to `_NET_SYSTEM_TRAY_S0`, and exits when you
click the last test icon. It is a test tool, not an installed component.
