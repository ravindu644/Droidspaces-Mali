#!/bin/sh
# Builds the Fedora packages. Run as root in a fedora container of the target release.
#   build.sh mesa     droidspaces-mali-mesa.spec, Mesa 25.1 with patches/mesa-25.1 for apps
#   build.sh kwin     Fedora's own KWin with Anland's backend and patches/kwin on top
#   build.sh hybris   droidspaces-mali.spec, needs shim/libui_compat_layer.so built first
# RPMs land in ~/rpmbuild/RPMS.
set -eu
repo=$(cd "$(dirname "$0")/../.." && pwd)
src=$HOME/rpmbuild/SOURCES
dnf -y install rpm-build dnf5-plugins git
mkdir -p "$src"

# Adds the patches to a spec in the order given. rpm applies Patch tags in the order the
# spec lists them, so they go in as one block after Release.
add_patches() {
    spec=$1
    shift
    n=9000
    : > /tmp/patch-tags
    for p in "$@"; do
        n=$((n + 1))
        cp "$p" "$src/"
        echo "Patch$n: $(basename "$p")" >> /tmp/patch-tags
    done
    sed -i '/^Release:/r /tmp/patch-tags' "$spec"
}

case ${1:-} in
mesa)
    spec=$repo/packages/fedora/droidspaces-mali-mesa.spec
    cp "$repo"/patches/mesa-25.1/*.patch "$src/"
    set --
    ;;
kwin)
    git clone -q --depth 1 -b legacy https://github.com/ravindu644/anland /tmp/anland
    kde=/tmp/anland/producers/kde
    dnf -y download --srpm --destdir /tmp kwin
    rpm -i /tmp/kwin-*.src.rpm
    spec=$HOME/rpmbuild/SPECS/kwin.spec
    # The backend lives outside KWin's tree, so it goes in as its own source and is
    # unpacked into src/backends/anland once %autosetup has applied the patches
    tar -chf "$src/anland-backend.tar" -C "$kde/anland_backend/src/backends/anland" .
    sed -i '/^Source0:/a Source999: anland-backend.tar' "$spec"
    sed -i '/^%autosetup -p1$/a mkdir -p src/backends/anland \&\& tar xf %{SOURCE999} -C src/backends/anland' "$spec"
    add_patches "$spec" "$kde"/Fedora43_v5/kwin.patch "$repo"/patches/kwin/*.patch
    set -- --define "dist $(rpm -E %dist).mali1" --define 'debug_package %{nil}'
    ;;
hybris)
    spec=$repo/packages/fedora/droidspaces-mali.spec
    cp "$repo"/patches/libhybris/*.patch "$repo"/shim/compat.h "$repo"/shim/libui_compat_layer.so \
       "$repo"/session/scrubenv.c "$repo"/session/kwin_wayland "$repo"/session/mali-gpu.env \
       "$repo"/session/plasma-mali@.service "$repo"/session/llvmpipe-run "$src/"
    set --
    ;;
*)
    echo "usage: $0 mesa|kwin|hybris" >&2
    exit 1
    ;;
esac

dnf -y builddep "$spec"
# Source downloads retry, a single TLS hiccup on a mirror shouldn't fail the build
rpmbuild -bb --undefine _disable_source_fetch \
    --define '__urlhelper_localopts --retry 5 --retry-all-errors --connect-timeout 30' \
    "$@" "$spec"
