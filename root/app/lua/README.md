# Lua

This project builds the Lua 5.4.8 interpreter and bytecode compiler for
CreateOS. The upstream source is kept under
`kernel/lib/third_party/lua-5.4.8/`; its license and copyright notice are in
that source tree.

Build the interpreter project with:

```sh
make -C rootfs_maker/root/app/lua
```

The normal userland build installs `lua.APPL` and `luac.APPL` under
`/app/system/bin/`, with `lua` and `luac` command wrappers.

```text
lua -v
lua -e print("hello")
lua /home/root/script.lua
luac -o /home/root/script.luac /home/root/script.lua
lua /home/root/script.luac
```

Starting `lua` without a script opens the interactive prompt. It accepts
multiline chunks; type `exit` or press Ctrl-C to leave.
The guest shell splits on whitespace and does not group quoted arguments,
so keep `-e` code to one token or enter it at the prompt.

## Runtime notes

The runtime opens Lua's base, coroutine, debug, I/O, math, OS, package,
string, table, and UTF-8 libraries. Source files can be loaded from the
current directory, the user's home, and `/usr/share/lua/5.4`. Script files
are limited to 1 MiB.

CreateOS is not a full POSIX host for Lua: native C modules are disabled,
`os.execute` and `io.popen` do not start programs, and `os.getenv` only
provides `HOME`, `PWD`, and `PATH`. Calendar functions use UTC. The guest
does not include a C compiler exposed as `cc`; `luac` compiles Lua bytecode,
not C.

The complete user-facing reference is in `docs/sdk/lua.md` in the full
CreateOS source tree.
