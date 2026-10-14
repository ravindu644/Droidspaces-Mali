%global debug_package %{nil}
%global hybris_commit 91f805b604f95089dd1ac476b93c19162490efc3
%global headers_commit 89db8ba815097af8a9c7650d2d5eefb378248839
# hybris' libEGL and libvulkan live under /opt/hy for KWin and the Vulkan directory only.
# They must never stand in for the system's libraries in another package's dependencies.
%global __provides_exclude_from ^/opt/hy/
%global __requires_exclude_from ^/opt/hy/

Name:           droidspaces-mali
Version:        1
Release:        1%{?dist}
Summary:        Mali GPU desktop session for Droidspaces: libhybris, gralloc shim and Plasma session
License:        Apache-2.0
URL:            https://github.com/ravindu644/Droidspaces-Mali
Source0:        https://github.com/droidian/libhybris/archive/%{hybris_commit}/libhybris-%{hybris_commit}.tar.gz
Source1:        https://github.com/Halium/android-headers/archive/%{headers_commit}/android-headers-%{headers_commit}.tar.gz
Source2:        compat.h
# Built with the NDK, it's a bionic library
Source3:        libui_compat_layer.so
Source4:        scrubenv.c
Source5:        kwin_wayland
Source6:        mali-gpu.env
Source7:        plasma-mali@.service
Source8:        llvmpipe-run

BuildRequires:  autoconf automake libtool gettext-devel pkgconf-pkg-config gcc gcc-c++ make patch
BuildRequires:  wayland-devel wayland-protocols-devel libdrm-devel mesa-libgbm-devel libglvnd-devel
BuildRequires:  vulkan-headers systemd-rpm-macros libstdc++-static

%description
libhybris with the Mali desktop patches, the AHardwareBuffer gralloc shim, and the systemd
unit, wrapper and environment that run KDE Plasma on the Mali GPU in a Droidspaces container.

%prep
%setup -q -n libhybris-%{hybris_commit} -a 1
for p in %{_sourcedir}/0*-hybris-*.patch; do patch -p1 -s < "$p"; done
# tests/ doesn't build against Android 14's headers
sed -i 's/^SUBDIRS += utils tests$/SUBDIRS += utils/' hybris/Makefile.am

%build
# Same flags as the tested build. Fedora's hardening flags were never tried with bionic.
unset CFLAGS CXXFLAGS LDFLAGS
cd hybris
NOCONFIGURE=1 ./autogen.sh
./configure --prefix=/opt/hy/inst --enable-arch=arm64 --enable-wayland --enable-lindroid-drm \
  --enable-mali-quirks --enable-debug --enable-property-cache=no \
  --with-android-headers=$PWD/../android-headers-%{headers_commit} \
  --with-default-hybris-ld-library-path=/vendor/lib64/egl:/vendor/lib64:/system/lib64:/apex/com.android.vndk.v33/lib64
%make_build CFLAGS="-O1 -g -DDEBUG -include %{SOURCE2}" CXXFLAGS="-O1 -g -DDEBUG -include %{SOURCE2}"
gcc -O2 -Wall -shared -fPIC -o scrubenv.so %{SOURCE4} -ldl

%install
# hybris' libraries find each other through their /opt/hy/inst/lib runpath. That is
# deliberate, so tell the rpath check (error 0x0002 is a runpath outside the system dirs).
export QA_RPATHS=$(( 0x0002 ))
make -C hybris install DESTDIR=%{buildroot}
find %{buildroot} -name '*.la' -delete
mkdir -p %{buildroot}/opt/hy/vk
for l in libvulkan.so.1 libOpenCL.so.1 libhybris-common.so.1 libhardware.so.2 libandroid-properties.so.1; do
    ln -s /opt/hy/inst/lib/$l %{buildroot}/opt/hy/vk/
done
ln -s libvulkan.so.1 %{buildroot}/opt/hy/vk/libvulkan.so
mkdir -p %{buildroot}/opt/hy/egl
for l in libEGL.so.1 libGLESv2.so.2 libGLESv1_CM.so.1; do
    ln -s /opt/hy/inst/lib/$l %{buildroot}/opt/hy/egl/
done
ln -s libOpenCL.so.1 %{buildroot}/opt/hy/vk/libOpenCL.so
install -Dm644 %{SOURCE3} %{buildroot}/opt/hy/android/libui_compat_layer.so
install -Dm755 hybris/scrubenv.so %{buildroot}/opt/hy/bin/scrubenv.so
install -Dm755 %{SOURCE5} %{buildroot}/opt/hy/bin/kwin_wayland
install -Dm644 %{SOURCE6} %{buildroot}/opt/hy/etc/mali-gpu.env
# KWin 6.7's Qt Quick scenes (Overview, Present Windows) crash the Mali driver on their
# first texture upload, so they render on the CPU and their window previews stay empty
echo 'export QT_QUICK_BACKEND=software' > %{buildroot}/opt/hy/etc/kwin.env
# Firefox's PBO texture uploads arrive empty through Zink on the Mali: text and icons
# render as white boxes. Plain uploads work.
mkdir -p %{buildroot}%{_libdir}/firefox/browser/defaults/preferences
echo 'pref("gfx.webrender.pbo-uploads", false);' \
    > %{buildroot}%{_libdir}/firefox/browser/defaults/preferences/droidspaces-mali.js
install -Dm644 %{SOURCE7} %{buildroot}%{_unitdir}/plasma-mali@.service
install -Dm755 %{SOURCE8} %{buildroot}%{_bindir}/llvmpipe-run

%files
%dir /opt/hy
%dir /opt/hy/etc
/opt/hy/inst
/opt/hy/vk
/opt/hy/egl
/opt/hy/android
/opt/hy/bin
# Other phones change the VNDK version in it
%config(noreplace) /opt/hy/etc/mali-gpu.env
/opt/hy/etc/kwin.env
%{_libdir}/firefox/browser/defaults/preferences/droidspaces-mali.js
%{_unitdir}/plasma-mali@.service
%{_bindir}/llvmpipe-run
