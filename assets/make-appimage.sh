#!/bin/sh

set -eu

pacman -Syu --needed --noconfirm \
    base-devel meson gtk4 glib-networking gvfs libx11 jansson luajit openssl \
    desktop-file-utils patchelf wget xorg-server-xvfb zsync

tools_url=https://raw.githubusercontent.com/pkgforge-dev/Anylinux-AppImages/refs/heads/main/useful-tools
wget -O /tmp/get-debloated-pkgs.sh "$tools_url/get-debloated-pkgs.sh"
sh /tmp/get-debloated-pkgs.sh --add-common --prefer-nano
wget -O /tmp/quick-sharun "$tools_url/quick-sharun.sh"
chmod +x /tmp/quick-sharun

# Build against the same libraries that quick-sharun will bundle.
meson setup build-appimage --buildtype=release --prefix=/usr
meson compile -C build-appimage
meson install -C build-appimage

export APPDIR="$PWD/AppDir"
export OUTPATH="$PWD"
export OUTNAME="$APPIMAGE_NAME"
export DESKTOP=/usr/share/applications/org.libresplit.LibreSplit.desktop
export ICON=/usr/share/icons/hicolor/256x256/apps/libresplit.png
export ANYLINUX_LIB=1
export DEPLOY_GLIB_NETWORKING=1

rm -rf "$APPDIR"

# Container runs as root so bypass protection for the test run.
BYPASS_ROOT_PROTECTION_CHECKS=1 /tmp/quick-sharun /usr/bin/libresplit /usr/bin/libresplit-ctl
cp assets/appimage.sh "$APPDIR/AppRun.sh"
/tmp/quick-sharun --make-appimage
