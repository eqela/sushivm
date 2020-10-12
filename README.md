The Sushi Virtual Machine
=========================

Overview
--------

Sushi is a fast and lightweight JIT-enabled virtual machine and dynamic language interpreter.
It embeds and utilizes the LuaJIT interpreter and JIT engine and implements a fully custom-built
cross-platform built-in API for Sushi applications. The entire virtual machine runs from
a single executable and requires no particular installation. Executable Sushi programs can
be compressed and appended to the executable, easily creating single-file executables that
contain both the interpreter and application code. The executable size is about 400kb
when dynamically linked and around 4MB when linked statically.

Sushi uses the Lua syntax as its native code format. While it is possible to
write and execute hand-written Lua code on Sushi as well, the primary purpose of
Sushi is to provide an optimal runtime environment for the
[Sling Programming Language](http://eqdn.tech/sling).

Sushi runs on Linux, macOS and Windows.

Installation
------------

Release builds of Sushi for all supported platforms are found on the
[release page](https://github.com/eqela/sushivm/releases).

On MacOS and Linux, Sushi can be conveniently installed and updated by running the installation
script from the command line terminal as follows:

```
curl -o- https://raw.githubusercontent.com/eqela/sushivm/master/install.sh | sh
```

The installation script updates PATH and also updates to the latest version if an older version
is already installed.
