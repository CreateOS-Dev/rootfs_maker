# HTTP diagnostic app

`httpd.APPL` is a small test program for CreateOS's TCP listener API. It
accepts one HTTP request on port 8080, sends a fixed response, and exits. It
is useful for checking that a network device, listener, and app build are
working together. It is not a general web server.

Build it from the repository root:

```sh
make -C rootfs_maker/root/app/httpd
```

The normal userland build stages it at `/app/system/bin/httpd.APPL`. Start it
from an administrator system session:

```text
open httpd.APPL
```

It answers `GET /` and `GET /health`; other paths return 404, and methods
other than GET return 405. To connect from the host through QEMU's user
network, add a host-to-guest port forward for TCP port 8080.

The listener API is restricted to system-privileged apps and supports one
listener and one accepted stream at a time. CreateOS does not currently
provide a general inbound firewall. Keep this diagnostic app off untrusted
networks.
