%if %{undefined libresplit_version}
%{error:The build must define libresplit_version as X.Y.Z}
%endif
%if %{undefined libresplit_release}
%{error:The build must define libresplit_release as a positive integer}
%endif

Name:           libresplit
Version:        %{libresplit_version}
Release:        %{libresplit_release}
Summary:        Free speedrun timer with auto splitting and load removal for Linux.
License:        GPL-3.0-only
URL:            https://libresplit.org/
Source0:        %{name}-%{version}.tar.gz
Source1:        libresplit.repo
Source2:        org.libresplit.LibreSplit.metainfo.xml

ExclusiveArch:  x86_64 aarch64

BuildRequires:  appstream
BuildRequires:  binutils
BuildRequires:  desktop-file-utils
BuildRequires:  gcc
BuildRequires:  gtk3-devel
BuildRequires:  jansson-devel
BuildRequires:  libX11-devel
BuildRequires:  luajit-devel
BuildRequires:  meson
BuildRequires:  openssl-devel

Requires:       hicolor-icon-theme
Recommends:     glib-networking
Recommends:     gvfs

%description
LibreSplit is a speedrun timer based on urn that adds support for Lua-based auto
splitters that are easy to port from ASL.

%prep
%autosetup -n %{name}-%{version}
sed -i "s/version: 'pre-release'/version: '%{version}'/" meson.build
grep -Fq "version: '%{version}'" meson.build

%build
%meson
%meson_build

%install
%meson_install
install -Dpm 0644 %{SOURCE2} \
    %{buildroot}%{_metainfodir}/org.libresplit.LibreSplit.metainfo.xml
install -Dpm 0644 %{SOURCE1} \
    %{buildroot}%{_sysconfdir}/yum.repos.d/libresplit.repo

%check
desktop-file-validate \
    %{buildroot}%{_datadir}/applications/libresplit.desktop
appstreamcli validate --no-net \
    %{buildroot}%{_metainfodir}/org.libresplit.LibreSplit.metainfo.xml

%files
%license %{_licensedir}/%{name}/LICENSE
%doc %{_docdir}/%{name}/README.md
%{_bindir}/libresplit
%{_bindir}/libresplit-ctl
%{_datadir}/applications/libresplit.desktop
%{_datadir}/icons/hicolor/*/apps/libresplit.png
%{_metainfodir}/org.libresplit.LibreSplit.metainfo.xml
%config(noreplace) %{_sysconfdir}/yum.repos.d/libresplit.repo

%changelog
* Mon Aug 03 2026 LibreSplit <rpm@libresplit.org> - %{version}-%{release}
- Add the initial LibreSplit RPM packaging policy.
