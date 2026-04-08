# How to build the universal tooling

## Required Files

You will need [Cosmopolitan Libc](https://github.com/jart/cosmopolitan).

## Drawing the Owl

From Luigi: to do this I used meson. The trick was point meson to the compiler as a cross compiler

```ini
[constants]
path = '/opt/cosmos/bin'

[binaries]
c = path / 'x86_64-unknown-cosmo-cc'
cpp = path / 'x86_64-unknown-cosmo-c++'
objc = path / 'x86_64-unknown-cosmo-gcc'
ar = path / 'x86_64-unknown-cosmo-ar'
as = path / 'x86_64-unknown-cosmo-as'
strip = path / 'x86_64-unknown-cosmo-strip'
pkg-config = path / 'x86_64-unknown-cosmo-pkg-config'
windres = path / 'x86_64-unknown-cosmo-windres'

[properties]
# Directory that contains 'bin', 'lib', etc
root = '/opt/cosmos'
# Directory that contains 'bin', 'lib', etc for the toolchain and system libraries
sys_root = '/opt/cosmos/x86_64-linux-cosmo'
needs_exe_wrapper = false
```

This is a meson cross-file.ini and it sets up the cross toolchain. However, this fails, as meson fails to execute ar. To fix that you have to wrap `*-unknown-cosmo-ar` in a shell script that passes the arguments to trick python:

```bash
#!/usr/bin/env sh
exec /opt/cosmos/bin/x86_64-unknown-cosmo-ar $@
```

Point meson to that and it works again. You have to do this for both x86_64 and ARM64 separately. It's easier to use cosmocc with autotools for things that use autoconf/make. CMake would need a similar cross file to meson.

## The rest of the Owl

You now have strange binaries in your build directories.

You have to run some scripts from cosmos next:

```bash
export PATH=/opt/cosmos/bin:$PATH
fixupobj build-x86_64/countchan  
fixupobj build-arm/countchan
apelink -o countchan -M /opt/cosmos/bin/ape-m1.c -l /opt/cosmos/bin/ape-aarch64.elf -l /opt/cosmos/bin/ape-x86_64.elf build-x86_64/countchan build-arm/countchan
pecheck countchan
```

And bam, it works
