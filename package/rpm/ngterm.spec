Name:        ngterm
Version:     1.0
Release:     1%{?dist}
Summary:     A modern terminal emulator with connection management
License:     MIT
URL:         https://yourwebsite.com
Source0:     %{name}-%{version}.tar.gz

BuildRequires: gtkmm3-devel, vte3-devel, nlohmann-json-devel
Requires:    gtkmm3, vte3, nlohmann-json

%description
A modern terminal emulator with connection management features.

%prep
%setup -q

%build
%cmake .
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%files
%{_bindir}/ngterm
%{_includedir}/icondata.h

%changelog
* Tue May 16 2024 Jayan Menon <jmoolayil@gmail.com> - 1.0-1
- Initial release
