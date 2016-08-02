Summary:	Warp - language detector and lexical processor engine
Name:		greylock
Version:	0.1.0
Release:	1%{?dist}.1

License:	GPLv3
Group:		System Environment/Libraries
URL:		http://reverbrain.com/
Source0:	%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)


BuildRequires:	ribosome-devel
BuildRequires:	libswarm3-devel, libthevoid3-devel
BuildRequires:	boost-devel, boost-system, boost-program-options, boost-thread
BuildRequires:	msgpack-devel
BuildRequires:	cmake >= 2.6

%description
Warp is a lexical processor engine, it supports statistical language detection and word stemming.
It implementes high-performace HTTP server as well as pure C++ classes in its headers.
There is also testing application which allows to gather language statistics.
Go bindings are created for HTTP client, it supports tokenization and stemming conversions.

%package devel
Summary: Development files for %{name}
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}


%description devel
Header files for WARP lexical processor.

%prep
%setup -q

%build
export LDFLAGS="-Wl,-z,defs"
export DESTDIR="%{buildroot}"
%{cmake} .
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR="%{buildroot}"

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_bindir}/warp_server
%{_bindir}/warp_detector
%doc conf/


%files devel
%defattr(-,root,root,-)
%{_includedir}/*

%changelog
* Tue Mar 19 2013 Evgeniy Polyakov <zbr@ioremap.net> - 0.1.0
- Initial release
