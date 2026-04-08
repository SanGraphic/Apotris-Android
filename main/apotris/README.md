# Apotris

<https://apotris.com>

You can find builds/releases at https://apotris.com/downloads

Apotris is a block stacking game for the Gameboy Advance! It features satisfying graphics, responsive controls and a large amount of customization so that you can tailor the game to your preferences!

Join the [discord server](https://discord.com/invite/jQnxmXS7tr) for updates, beta releases, or to give me feedback!

## Docker build

Want to bypass all the work and use a system shipped by the developers intended to build Apotris? Try the docker image:

First, [install Docker](https://docs.docker.com/get-docker/) for your system. Then get it running and open up a terminal:

```bash
# Download and log in to the Apotris builder docker image
docker run -it docker.io/akouzoukos/apotris bash
# OR Optional: build the image yourself
docker build -t apotris-builder . && docker run -it apotris-builder bash
# You are now in the docker image environment, use Ctrl+D or "exit" command to return to main OS

# Clone the latest sources
git clone --recursive https://gitea.com/akouzoukos/apotris.git
# Enter source dir
cd apotris
# From here you can build one of the available targets

# GBA
meson setup --cross-file=meson/gba.ini build-gba && meson compile -C build-gba
# Windows
meson setup --cross-file=meson/mingw.ini build-windows && meson compile -C build-windows
# Linux
meson setup build-linux && meson compile -C build-linux

# NOTE: If you're trying to produce a build exactly like the released builds, you want to add flags as well like below
# Some emulators and platforms will behave differently without these flags (e.g. a debug build)
meson setup -Db_lto=true -Db_lto_mode=thin --buildtype=release ...
# Also note that some builds (specifically the Windows build) may fail with the LTO-related flags, so remove those if the sources no longer compile

# Built executables are now in the Docker environment's apotris/build-*/ folders
```

OR mount the repo as a volume:

```bash
# Clone the repo locally (add --config core.autocrlf=false if you are on Windows)
git clone --recursive https://gitea.com/akouzoukos/apotris.git 
# Enter source dir
cd apotris 
# Build the image
docker build -t apotris-builder . 
# Run the image with the project directory mounted
docker run -it -v ${PWD}/:/root/apotris -w /root/apotris apotris-builder bash

# GBA
meson setup --cross-file=meson/gba.ini build-gba && meson compile -C build-gba
# Windows
meson setup --cross-file=meson/mingw.ini build-windows && meson compile -C build-windows
# Linux
meson setup build-linux && meson compile -C build-linux
# Built executables are now in the local environment's apotris/build-*/ folders
```

## Meson build

### Overview of requirements for most systems

As all the libraries and tools are compiled from source, the only things you
need to have installed and in your PATH are:

- meson (at least version 1.3.0 or later)
  - If your system has an out of date meson, install `pip` and run `pip install -U meson` to get the latest
- git
- A compiler for your system (`gcc`, `clang`, etc.)
- A compiler for the GBA (`arm-none-eabi-gcc`)
  - On Windows you can download the [.exe installer](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) for `mingw-w64-i686-arm-none-eabi-gcc`
  - On Ubuntu download at least 13.2 of arm-none-eabi-gcc for your system
    [here](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) 
    - Extract the archive into your system path with `tar xf ./arm-gnu-toolchain-*-arm-none-eabi.tar.xz -C /usr/share/`
    - Make sure to symlink everything `sudo ln -sf /usr/share/arm-gnu-toolchain-YOUR_VERSION/bin/* /usr/bin/`

**Note:** On macOS one can `brew install --cask gcc-arm-embedded && brew install meson ninja opus pkg-config cmake` to get all required deps.

### Windows (native) requirements

You will need to install:

- [Python 3](https://www.python.org/downloads/windows/)
  - get the latest stable Windows installer for your platform
  - ensure you "add to PATH" when you run the installer (unless you know what you're doing otherwise)
- Meson
  - Run `pip install meson` after installing Python to get meson
  - An alternative is [the Windows installer](https://github.com/mesonbuild/meson/releases/download/1.4.0/meson-1.4.0-64.msi)
- A Windows-native compiler and build tools
  - [Strawberry Perl](https://strawberryperl.com) works out of the box, get the system installer to make it easiest

#### Windows GBA build

For the GBA target, you will need to update the `meson/gba.ini` file to have the path to your ARM compiler:

```ini
[constants]
path = 'C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.2 Rel1\bin'
# ... rest of original file
```

You can avoid editing a file by setting this with a powershell command per-session

```powershell
$env:PATH="$env:PATH;C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.2 Rel1\bin"
```

Ensure the version string and directory paths are correct for your installation. They may change with future updates.

### Ubuntu 22.04+ and Windows (WSL) dependencies

#### For all Linux systems

Install the following packages:

```bash
sudo apt install build-essential git meson cmake 
```

also install `mingw-w64` if you plan to cross compile for Windows

#### For Linux native builds

Install the following dependencies of SDL2:

```bash
sudo apt-get install build-essential git make \
  pkg-config cmake ninja-build gnome-desktop-testing libasound2-dev libpulse-dev \
  libaudio-dev libjack-dev libsndio-dev libx11-dev libxext-dev \
  libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev \
  libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev \
  libegl1-mesa-dev libdbus-1-dev libibus-1.0-dev libudev-dev fcitx-libs-dev libglew-dev
```

**Note:** On some Ubuntu installations the SDL2 libraries are not imported correctly by the build. Removing `libsdl2-dev` may resolve these issues. 

## Compiling

Once you have everything, `git clone --recursive https://gitea.com/akouzoukos/apotris.git` or [download](https://gitea.com/akouzoukos/apotris/archive/main.zip) this repository, then
navigate to the directory in a terminal (or "command-line") window, and run some or all of the
following commands depending on your desired target:

```sh
# Native build
meson setup build
meson compile -C build

# GBA Build
meson setup --cross-file=meson/gba.ini build-gba
meson compile -C build-gba

# MingW build (Windows using Linux, WSL, or macOS to build)
# Note: the following command is a really bad hack that probably has no consequences but is required
sudo ln -sf $(which x86_64-w64-mingw32-g++-posix) $(which x86_64-w64-mingw32-g++)
meson setup --cross-file=meson/mingw.ini build-mingw
meson compile -C build-mingw
```

The built Apotris ROMs (.elf, .gba) will be in your `build-gba` directory.

The built Apotris executable and its required `assets/` folder will be in your build directories for other builds.

## Less common platforms

### Nintendo Switch

The Switch build requires one have the DevkitARM A64 toolchain installed. Installing this is covered by [this reference](https://devkitpro.org/wiki/Getting_Started). You will want the `switch-dev` and `switch-portlibs` targets installed.

Ensure the compiler is in your PATH and run the cross build:

```bash
meson setup --cross-file=meson/switch.ini build-switch
meson compile -C build-switch
```

The file is output to `build-switch/Apotris.nro`.

### Emscripten

The Emscripten build requires one to have the Emscripten toolchain/SDK installed. See [this reference](https://emscripten.org/docs/getting_started/downloads.html). The latest should work, but at least `3.1.51` is required.

The Emscripten build has a bug. The workaround is relatively simple.

```bash
$ meson setup --cross-file=meson/emscripten.ini build-web

$ meson compile -C build-web # this command will throw an error:

AssertionError: attempt to lock the cache while a parent process is holding the lock (sysroot/lib/wasm32-emscripten/libSDL2.a)
em++: error: subprocess 1/34 failed (returned 1)! (cmdline: /opt/homebrew/Cellar/emscripten/3.1.58/libexec/emcc -c /opt/homebrew/Cellar/emscripten/3.1.58/libexec/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0/src/mixer.c -o /opt/homebrew/Cellar/emscripten/3.1.58/libexec/cache/ports-builds/sdl2_mixer/src/mixer.c.o -g -sSTRICT -Werror -O2 -I/opt/homebrew/Cellar/emscripten/3.1.58/libexec/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0 -sUSE_SDL=2 -O2 -DMUSIC_WAV -sUSE_VORBIS -DMUSIC_OGG -I/opt/homebrew/Cellar/emscripten/3.1.58/libexec/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0/include -I/opt/homebrew/Cellar/emscripten/3.1.58/libexec/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0/src -I/opt/homebrew/Cellar/emscripten/3.1.58/libexec/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0/src/codecs)
ninja: build stopped: subcommand failed.
```

Take the command it failed on (in the `cmdline:` section) and run it manually:

```bash
$ /opt/homebrew/Cellar/emscripten/3.1.58/libexec/emcc -c /opt/homebrew/Cellar/emscripten/3.1.58/libexec/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0/src/mixer.c -o /opt/homebrew/Cellar/emscripten/3.1.58/libexec/cache/ports-builds/sdl2_mixer/src/mixer.c.o -g -sSTRICT -Werror -O2 -I/opt/homebrew/Cellar/emscripten/3.1.58/libexec/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0 -sUSE_SDL=2 -O2 -DMUSIC_WAV -sUSE_VORBIS -DMUSIC_OGG -I/opt/homebrew/Cellar/emscripten/3.1.58/libexec/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0/include -I/opt/homebrew/Cellar/emscripten/3.1.58/libexec/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0/src -I/opt/homebrew/Cellar/emscripten/3.1.58/libexec/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0/src/codecs

cache:INFO: generating port: sysroot/lib/wasm32-emscripten/libSDL2.a... (this will be cached in "/opt/homebrew/Cellar/emscripten/3.1.58/libexec/cache/sysroot/lib/wasm32-emscripten/libSDL2.a" for subsequent builds)
system_libs:INFO: compiled 117 inputs in 2.99s
cache:INFO:  - ok
```

Then re-run the compile step again:

```bash
$ meson compile -C build-web

ninja: Entering directory `build-web`
[1/1] Linking target Apotris.html # success !
```

### Portmaster

You'll need to load the portmaster development environment, covered [here](https://portmaster.games/docker.html).

Once inside the container, clone the repo and configure as if it were a native build, but with one key config:

```bash
meson setup -Dportmaster=true build-pm
meson compile -C build-pm
```

For some platforms (namely the TrimUI Smart Pro or other devices with specifically a Cortex A-53) you'll need to add the native-file argument to bring in some patches for hardware bugs.

```bash
meson setup --native-file=meson/portmaster.ini -Dportmaster=true build-pm
meson compile -C build-pm
```

**Note:** on typical Intel/AMD machines this can take a *long* time.


### Nintendo 3DS

Compiling the 3DS build requires you to install the devkitARM toolchain from devkitPro. You can refer to the [3DBrew](https://www.3dbrew.org/wiki/Setting_up_Development_Environment) and [devkitPro](https://devkitpro.org/wiki/Getting_Started) guides to set it up. Following the guide, you should install all packages in the package group `3ds-dev` using `pacman` (or `dkp-pacman`).

You will also need to install the library `libmpg123`, `libvorbisidec`, `libxmp` and `opusfile` using `pacman` (or `dkp-pacman`):

```bash
pacman -S 3ds-mpg123 3ds-libvorbisidec 3ds-libxmp 3ds-opusfile
```

To perform the cross build:

```bash
meson setup --cross-file=meson/n3ds.ini build-n3ds
meson compile -C build-n3ds
```

The build output file can be found as `build-n3ds/Apotris.3dsx`.

To run the build, the audio assets must be placed on the SD card under `/3ds/Apotris/assets/` beforehand.

#### Building a CIA file

Normally, there is no reason to use a CIA file. Especially if you are developing and debugging code, the 3dsx file can be easily loaded using `3dslink` which also allows you to see stdout and stderr output with its server.

To build the CIA, you will need to install [`bannertool`](https://github.com/Epicpkmn11/bannertool) and [`makerom`](https://github.com/3DSGuy/Project_CTR/tree/master/makerom) manually and have them available on the `PATH` environment variable while building.

```bash
meson compile -C build-n3ds cia
```

**Metadata**:

* Unique ID: `0xA9715`
* Title ID: `000400000A971500`
* Product Code: `CTR-H-APTR`
