%global debug_package %{nil}
%global _lto_cflags %{nil}
# A second Mesa for apps only, under /opt/hy/mesa. Its libraries must never stand in for
# the system Mesa in another package's dependencies, nor need each other from the system.
%global __provides_exclude_from ^/opt/hy/
%global __requires_exclude ^lib(gallium-|glapi|EGL_mesa|GLX_mesa|gbm)

Name:           droidspaces-mali-mesa
Version:        25.1.9
Release:        1%{?dist}
Summary:        Mesa with Zink for the Mali, for apps in the Droidspaces Mali session
License:        MIT
URL:            https://github.com/ravindu644/Droidspaces-Mali
# archive.mesa3d.org goes down at times, the GitLab tag archive is the same tree
Source0:        https://gitlab.freedesktop.org/mesa/mesa/-/archive/mesa-%{version}/mesa-mesa-%{version}.tar.gz
Patch1:         0001-kopper-add-LIBGL_KOPPER_DISABLE_X11-an-X11-only-kopp.patch
Patch2:         0002-zink-don-t-split-fragment-inputs-by-component-on-gap.patch
Patch3:         0003-zink-advertise-instance-divisors-without-VK_EXT_vert.patch
# Upstream 179e744f: glibc 2.43 declares C23 call_once itself
Patch4:         0004-c11-threads-fix-build-on-c23.patch

BuildRequires:  meson gcc gcc-c++ bison flex python3-mako python3-pyyaml python3-packaging
BuildRequires:  libdrm-devel wayland-devel wayland-protocols-devel libglvnd-devel expat-devel
BuildRequires:  libX11-devel libxcb-devel libXext-devel libXfixes-devel libXxf86vm-devel
BuildRequires:  libXrandr-devel libxshmfence-devel zlib-devel libzstd-devel

%description
Mesa 25.1, the last release whose Zink runs without VK_EXT_robustness2's nullDescriptor,
which the Mali lacks. Built with Zink only and installed under /opt/hy/mesa, so the system
Mesa stays as Fedora ships it. Apps reach it through /opt/hy/vk.

%prep
%autosetup -p1 -n mesa-mesa-%{version}

%build
# Our libraries find each other through the runpath, so a system Mesa of another
# version can't satisfy them halfway
export LDFLAGS="${LDFLAGS:-} -Wl,-rpath,/opt/hy/mesa/lib"
meson setup build --prefix=/opt/hy/mesa --libdir=lib --buildtype=release \
  -Dgallium-drivers=zink -Dvulkan-drivers= -Dllvm=disabled -Dplatforms=x11,wayland \
  -Dglvnd=enabled -Dglx=dri -Degl=enabled -Dgbm=enabled -Dgles1=disabled -Dgles2=enabled \
  -Dgallium-va=disabled -Dgallium-vdpau=disabled -Dgallium-xa=disabled -Dgallium-nine=false \
  -Dgallium-opencl=disabled -Dgallium-rusticl=false -Dvideo-codecs= -Dvalgrind=disabled \
  -Dlibunwind=disabled -Dlmsensors=disabled -Dbuild-tests=false -Dzstd=enabled
meson compile -C build %{?_smp_mflags}

%install
# 0x0002 is a runpath outside the system dirs, which is the point here
export QA_RPATHS=$(( 0x0002 ))
DESTDIR=%{buildroot} meson install -C build --no-rebuild
# glvnd finds the vendor libraries through the system's own vendor files
rm -rf %{buildroot}/opt/hy/mesa/share/glvnd %{buildroot}/opt/hy/mesa/include \
       %{buildroot}/opt/hy/mesa/lib/pkgconfig
mkdir -p %{buildroot}/opt/hy/vk
for l in libEGL_mesa.so.0 libGLX_mesa.so.0 libgbm.so.1; do
    ln -s /opt/hy/mesa/lib/$l %{buildroot}/opt/hy/vk/
done

%files
/opt/hy/mesa
/opt/hy/vk/libEGL_mesa.so.0
/opt/hy/vk/libGLX_mesa.so.0
/opt/hy/vk/libgbm.so.1
