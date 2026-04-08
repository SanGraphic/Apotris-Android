# GBA
meson setup --reconfigure --cross-file=meson/gba.ini -Db_lto=true -Db_lto_mode=thin --buildtype=release build-gba && \
ninja -C build-gba Apotris-pocket.gba

# Linux (native, actually)
meson setup -Db_lto=true -Db_lto_mode=thin --reconfigure --buildtype=release build-linux && \
meson compile -C build-linux && \
strip build-linux/Apotris

# Windows
meson setup --reconfigure --cross-file=meson/mingw.ini -Db_lto=false --buildtype=release build-win && \
meson compile -C build-win && \
x86_64-w64-mingw32-strip build-win/Apotris.exe

# Switch
export PATH=$PATH:/opt/devkitpro/tools/bin/
meson setup --reconfigure --cross-file=meson/switch.ini -Db_lto=false --buildtype=release build-switch &&\
ninja -C build-switch

# Web
meson setup --reconfigure --cross-file=meson/emscripten.ini -Db_lto=false --buildtype=release build-web && \
ninja -C build-web || true && \
/root/emsdk/upstream/emscripten/emcc -c /root/emsdk/upstream/emscripten/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0/src/effects_internal.c -o /root/emsdk/upstream/emscripten/cache/ports-builds/sdl2_mixer/src/effects_internal.c.o -g -sSTRICT -Werror -O2 -I/root/emsdk/upstream/emscripten/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0 -sUSE_SDL=2 -O2 -DMUSIC_WAV -sUSE_VORBIS -DMUSIC_OGG -I/root/emsdk/upstream/emscripten/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0/include -I/root/emsdk/upstream/emscripten/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0/src -I/root/emsdk/upstream/emscripten/cache/ports/sdl2_mixer/SDL_mixer-release-2.8.0/src/codecs
ninja -C build-web

# N3DS
meson setup --reconfigure --cross-file=meson/n3ds.ini --buildtype=release build-n3ds && \
meson compile -C build-n3ds 3dsx cia

# Dist
short_hash=$(git rev-parse --short HEAD)
current_branch=$(git branch | grep '*' | sed 's/* //')
gba=Apotris-GBA-$current_branch-$short_hash.zip
web=Apotris-Web-$current_branch-$short_hash.zip
nsw=Apotris-Switch-$current_branch-$short_hash.zip
lin=Apotris-Linux-x64-$current_branch-$short_hash.zip
win=Apotris-Windows-x64-$current_branch-$short_hash.zip
n3ds=Apotris-3DS-$current_branch-$short_hash.zip

cp tools/README_LINUX.txt build-linux/README.txt
cd build-linux && \
cp -r ../license . && \
zip $lin README.txt Apotris assets/* license/* -x assets/meson.build && \
cd ..

cd build-win && \
cp -r ../license . && \
zip $win Apotris.exe assets/* license/* -x assets/meson.build && \
cd ..

mkdir -p build-gba/audio && \
mkdir -p build-gba/tools && \
cp assets/* build-gba/audio/ && \
rm -rf build-gba/audio/meson.build build-gba/audio/favicon32.bmp && \
cp tools/ape.aarch64 build-gba/tools/ && \
cp tools/ape.x86_64 build-gba/tools/ && \
cp tools/gbfs.exe build-gba/tools/ && \
cp tools/buildSoundbank.exe build-gba/tools/ && \
cp tools/buildSoundbank.py build-gba/tools/ && \
cp tools/mmutil.exe build-gba/tools/ && \
cp tools/padbin.exe build-gba/tools/ && \
cp tools/cat.exe build-gba/tools/ && \
cp tools/countchan.exe build-gba/tools/ && \
cp tools/sound_effects.txt build-gba/tools/ && \
cp build-gba/Apotris-base.gba build-gba/tools/ && \
cp tools/README.txt build-gba/ && \
cp tools/buildSongs.bat build-gba/WINDOWS_build.bat && \
cp tools/buildSongs.sh build-gba/LINUX_or_MAC_build.sh && \
chmod +x build-gba/LINUX_or_MAC_build.sh
chmod +x build-gba/mmutil.exe build-gba/gbfs.exe build-gba/padbin.exe build-gba/cat.exe build-gba/countchan.exe
cd build-gba && \
cp -r ../license . && \
zip $gba LINUX_or_MAC_build.sh WINDOWS_build.bat README.txt Apotris.gba Apotris-pocket.gba tools/* audio/* license/* && \
cd ..

cd build-web && \
cp -r ../license . && \
zip $web Apotris.* license/* -x Apotris.html.p/ && \
cd ..

cp tools/README_SWITCH.txt build-switch/README.txt
cd build-switch && \
cp -r ../license . && \
zip $nsw README.txt Apotris.nro license/* assets/* -x assets/meson.build && \
cd ..

cd build-n3ds && \
mkdir -p 3ds/Apotris/ && \
cp -r assets 3ds/Apotris/ && \
rm -f 3ds/Apotris/assets/meson.build 3ds/Apotris/assets/favicon32.bmp && \
rm -rf 3ds/Apotris/assets/shaders && \
cp -r ../license-n3ds 3ds/Apotris/license && \
cp Apotris.3dsx 3ds/Apotris/ && \
cp Apotris.cia 3ds/Apotris/ && \
zip -r $n3ds.zip 3ds/Apotris && \
cd ..
