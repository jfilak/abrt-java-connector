%global commit 49eb1871274dbe8de78b426be4f77aa92720863d
%global shortcommit %(c=%{commit}; echo ${c:0:7})

Name:		abrt-java-connector
Version:	0.1.1
Release:	2%{?dist}
Summary:	JNI Agent library converting Java exceptions to ABRT problems

Group:		System Environment/Libraries
License:	GPLv2+
URL:		https://github.com/jfilak/abrt-java-connector

Source0:	https://github.com/jfilak/%{name}/archive/%{commit}/%{name}-%{version}-%{shortcommit}.tar.bz2

BuildRequires:	cmake
BuildRequires:	abrt-devel
BuildRequires:	java-1.7.0-openjdk-devel

Requires:	libreport-filesystem
Requires:	abrt

%description
JNI library providing an agent capable to process both caugh and uncaugh
exceptions and transform them to ABRT problems


%prep
%setup -qn %{name}-%{version}-%{shortcommit}


%build
%cmake -DABRT:BOOL=ON .
make %{?_smp_mflags}


%install
make install DESTDIR=%{buildroot}


%files
%doc LICENSE README AUTHORS
%config(noreplace) %{_sysconfdir}/libreport/plugins/bugzilla_format_java.conf
%config(noreplace) %{_sysconfdir}/libreport/plugins/bugzilla_formatdup_java.conf
%config(noreplace) %{_sysconfdir}/libreport/events.d/java_event.conf
%{_libdir}/lib%{name}.so


%changelog
* Mon Jun 03 2013 Jakub Filak <jfilak@redhat.com> - 0.1.1-2
- Provide ABRT configuration

* Mon Jun 03 2013 Jakub Filak <jfilak@redhat.com> - 0.1.1-1
- New release

* Fri May 31 2013 Jakub Filak <jfilak@redhat.com> - 0.1.0-3
- Build with the library name same as the package name

* Fri May 31 2013 Jakub Filak <jfilak@redhat.com> - 0.1.0-2
- Build with ABRT enabled

* Fri May 31 2013 Jakub Filak <jfilak@redhat.com> - 0.1.0-1
- Initial version
