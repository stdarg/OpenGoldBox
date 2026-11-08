#!/bin/sh
# Builds the Linux package for players inside an AlmaLinux 8 container, so the
# game needs only glibc 2.28, the same as Godot's own export templates, and
# runs on any x86_64 distribution from 2019 on (RHEL 8, Debian 10, Ubuntu 20.04).
# A build on the host links against the host's newer glibc and runs only on
# systems at least as new; see docs/BUILD.md, "The package for players".
#
#   tools/linux_release_build.sh [--test | --test-all]
#
# The first run creates the distrobox container opengold-alma8 and installs
# GCC 14 (gcc-toolset-14), CMake, Ninja, Python 3.11 and the libraries Godot
# needs. It then runs ./build.sh linux-release, which writes build/linux-release/
# and linux-package/ and fails if the package needs a glibc newer than 2.28.
# The container shares your home folder, so Godot, its export templates and
# OPENGOLD_GAME_DIR are the host's.
set -eu

cd "$(dirname "$0")/.."

CONTAINER=opengold-alma8
# docker.io's AlmaLinux images are kept current; quay.io's :8 is an old 8.7
# whose signing key AlmaLinux has since replaced.
IMAGE=docker.io/library/almalinux:8
TOOLSET=/opt/rh/gcc-toolset-14/root/usr/bin
PACKAGES="gcc-toolset-14-gcc-c++ gcc-toolset-14-binutils gcc-toolset-14-libstdc++-devel
    cmake ninja-build python3.11 git which tar
    fontconfig freetype libX11 libXcursor libXext libXi libXinerama libXrandr libXrender
    libxkbcommon libwayland-client libwayland-cursor libwayland-egl
    mesa-libGL mesa-libEGL mesa-dri-drivers vulkan-loader mesa-vulkan-drivers
    alsa-lib pulseaudio-libs dbus-libs systemd-libs"

if ! command -v distrobox >/dev/null 2>&1; then
    echo "distrobox not found. Bazzite ships it; elsewhere install distrobox and podman." >&2
    exit 1
fi

if ! distrobox list --no-color | grep -q "| $CONTAINER "; then
    distrobox create --yes --name "$CONTAINER" --image "$IMAGE"
fi

# rpm -q answers at once when everything is there, so only the first run waits on dnf.
# shellcheck disable=SC2086
distrobox enter "$CONTAINER" -- sh -c "rpm -q $(echo $PACKAGES) >/dev/null 2>&1 || {
    sudo dnf -y install dnf-plugins-core epel-release &&
    sudo dnf config-manager --set-enabled powertools &&
    sudo dnf -y install $(echo $PACKAGES); }"

# The host's PATH reaches into the container and would bring Homebrew's tools,
# which need the host's glibc, so only the container's own tools and
# ~/.local/bin (Godot) are on it.
distrobox enter "$CONTAINER" -- env \
    PATH="$TOOLSET:$HOME/.local/bin:/usr/local/bin:/usr/bin:/bin" \
    CC="$TOOLSET/gcc" CXX="$TOOLSET/g++" \
    ./build.sh linux-release "$@"
