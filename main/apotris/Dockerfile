FROM docker.io/devkitpro/devkita64:20240604 AS devkita64

FROM docker.io/devkitpro/devkitarm:latest AS devkitarm

FROM docker.io/library/ubuntu:24.04

# Install apt-available deps
RUN DEBIAN_FRONTEND=noninteractive \
  apt-get update \
  && apt-get install -y --no-install-recommends \
    python3 ash zsh vim build-essential git make curl meson xxd zstd wget sudo ca-certificates zip unzip \
    libsdl2-mixer-dev libopus-dev libvorbis-dev nodejs \
    pkg-config cmake ninja-build gnome-desktop-testing libasound2-dev libpulse-dev \
    libaudio-dev libjack-dev libsndio-dev libx11-dev libxext-dev \
    libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev \
    libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev \
    libegl1-mesa-dev libdbus-1-dev libibus-1.0-dev libudev-dev fcitx-libs-dev mingw-w64 \
  && apt-get install -y --fix-missing directx-headers-dev \
  && apt-get clean && rm -rf /var/lib/apt/lists/*

# Download and extract toolchain based on arch
RUN /bin/bash -c 'set -ex && \
    ARCH=`uname -m` && \
    if [ "$ARCH" == "x86_64" ]; then \
       echo "x86_64" && \
       curl -L "https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi.tar.xz?rev=e434b9ea4afc4ed7998329566b764309&hash=CA590209F5774EE1C96E6450E14A3E26" | tar xJ -C /usr/share; \
    else \
       echo "ARM64 arch?" && \
       curl -L "https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-aarch64-arm-none-eabi.tar.xz?rev=17baf091942042768d55c9a304610954&hash=06E4C2BB7EBE7C70EA4EA51ABF9AAE2D" | tar xJ -C /usr/share; \
    fi'

# Link toolchain into PATH
RUN ln -sf /usr/share/arm-gnu-toolchain-*/bin/* /usr/bin/

# Get missing windows deps and hack them in
RUN curl -L "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-dlfcn-1.4.1-1-any.pkg.tar.zst" | tar --strip-components=1 -x --zstd -C /usr/x86_64-w64-mingw32/

# Install latest meson 1.4.0
ADD https://github.com/mesonbuild/meson/releases/download/1.4.0/meson-1.4.0.tar.gz .
RUN tar xf meson-1.4.0.tar.gz && cd meson-1.4.0 && python3 setup.py install

# Install latest emscripten compiler
WORKDIR /root
RUN git clone --depth 1 --branch 4.0.5 https://github.com/emscripten-core/emsdk.git
WORKDIR emsdk
RUN ./emsdk install latest && ./emsdk activate latest && echo 'source "/root/emsdk/emsdk_env.sh"' >> $HOME/.bash_profile && echo 'source "/root/emsdk/emsdk_env.sh"' >> $HOME/.bashrc

# Return to root home folder
WORKDIR /root

# Install DevKitPro
COPY --from=devkita64 /opt/devkitpro /opt/devkitpro
COPY --from=devkitarm /opt/devkitpro /opt/devkitpro

ENV DEVKITPRO="/opt/devkitpro"
ENV DEVKITARM="$DEVKITPRO/devkitARM"
ENV DEVKITA64="$DEVKITPRO/devkitA64"
ENV PATH="$PATH:/$DEVKITPRO/tools/bin/"

# fix linux SDL2 xrandr dependency issue
RUN sudo apt remove --purge libsdl2-dev -y

# download bannertool
ARG BANNERTOOL_URL=https://github.com/diasurgical/bannertool/releases/download/1.2.0/bannertool.zip
ARG BANNERTOOL_ZIP_FILE=/tmp/bannertool.zip
ARG BANNERTOOL_INSTALL_PATH=/usr/local/bin/bannertool

RUN wget -O ${BANNERTOOL_ZIP_FILE} ${BANNERTOOL_URL} && \
    unzip ${BANNERTOOL_ZIP_FILE} -d /tmp && \
    mv /tmp/linux-x86_64/bannertool ${BANNERTOOL_INSTALL_PATH} && \
    chmod +x ${BANNERTOOL_INSTALL_PATH} && \
    rm ${BANNERTOOL_ZIP_FILE}

# download makerom
ARG MAKEROM_URL=https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.18.3/makerom-v0.18.3-ubuntu_x86_64.zip
ARG MAKEROM_ZIP_FILE=/tmp/makerom.zip
ARG MAKEROM_INSTALL_PATH=/usr/local/bin/makerom

RUN wget -O ${MAKEROM_ZIP_FILE} ${MAKEROM_URL} && \
    unzip ${MAKEROM_ZIP_FILE} -d /tmp && \
    mv /tmp/makerom ${MAKEROM_INSTALL_PATH} && \
    chmod +x ${MAKEROM_INSTALL_PATH} && \
    rm ${MAKEROM_ZIP_FILE}
