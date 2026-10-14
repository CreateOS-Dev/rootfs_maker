# CreateOS userland and root filesystem

`rootfs_maker` builds the programs that run after the kernel starts. It also
assembles the CFS5 root filesystem and provides host tools for creating and
checking disk images.

This directory is designed to sit beside `kernel/` and `sdk/` in a complete
CreateOS source tree. Some application Makefiles include kernel headers and
SDK rules using those relative paths.

## Build

From the repository root, build the configured cross-toolchain first, then
stage the userland:

```sh
make toolchain
make userapps
```

To work from this directory directly:

```sh
make -C rootfs_maker all
make -C rootfs_maker populate-rootfs
```

`all` builds the guest binaries under `build/rootfs_maker/`.
`populate-rootfs` stages files under `build/rootfs/`; it does not create the
persistent disk image. To create the CFS5 disk from that staged tree, run:

```sh
make disk-image
```

As part of staging, the build installs the SDK's public headers and static
libraries into the root filesystem's `/usr/include/createos/` and
`/usr/lib/createos/`.

The top-level build also packages the kernel, disk image, and GRUB files into
the bootable ISO.

## What's built

- `/sbin/init`, login, shell, and the first-boot setup program.
- Separate `.bin` files for guest commands.
- System `.APPL` applications from `root/app/*/Makefile`.
- TinyCC and Lua guest applications.
- Default configuration, keyboard and cursor assets, certificates, fonts,
  and runtime libraries.

Custom native apps placed in `root/appcustom/` are copied into `/app/custom/`
when the root filesystem is staged.

## Image tools

`make -C rootfs_maker host-tools` builds the host-side CFS5 utilities under
`build/rootfs_maker/host/`. `cfs5tool` can inspect and check an image:

```sh
build/rootfs_maker/host/cfs5tool info build/disk.img
build/rootfs_maker/host/cfs5tool check build/disk.img
```

`make clean` here removes staged rootfs-maker output; the top-level `make
clean` also removes `build/disk.img`. Back up any disk image containing data
you want to keep.

For the guest command and application list, see
`docs/userland/userland.md` in the complete CreateOS source tree.
