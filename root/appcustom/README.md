# Custom applications

Drop a finished `.APPL` or `.appl` file in this folder. When you build the
root filesystem, CreateOS copies it to `/app/custom/`; Explorer can launch it
from there.

From the repository root:

```sh
cp /path/to/my-app.APPL rootfs_maker/root/appcustom/
make userapps
make disk-image
make
```

To update an existing disk image after staging, use `make
refresh-system-apps`. Back up `build/disk.img` first if it contains files you
want to keep.

Apps built with the CreateOS SDK can also be installed into this folder with
their Makefile's `install` target. See `docs/sdk/app-development.md` for the
build steps.

These are native x86-64 executables, not sandboxed packages. Keep only apps
you trust here.
