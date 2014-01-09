%global commit befb850dbc72b117ad6bebabee314e7d41b97183
%global shortcommit %(c=%{commit}; echo ${c:0:7})

Name:		abrt-java-connector
Version:	1.0.6
Release:	1.rhbz1051198.1%{?dist}
Summary:	JNI Agent library converting Java exceptions to ABRT problems

Group:		System Environment/Libraries
License:	GPLv2+
URL:		https://github.com/jfilak/abrt-java-connector
Source0:	https://github.com/jfilak/%{name}/archive/%{commit}/%{name}-%{version}-%{shortcommit}.tar.gz

BuildRequires:	cmake
BuildRequires:	libreport-devel
BuildRequires:	java-1.7.0-openjdk-devel
BuildRequires:	systemd-devel

Requires:	abrt

%description
JNI library providing an agent capable to process both caught and uncaught
exceptions and transform them to ABRT problems


%prep
%setup -qn %{name}-%{commit}


%build
%cmake -DCMAKE_BUILD_TYPE=Release
make %{?_smp_mflags}


%install
make install DESTDIR=%{buildroot}

# Remove unwanted Fedora specific workflow configuration files
%if 0%{!?fedora:1}
rm -f $RPM_BUILD_ROOT/%{_datadir}/libreport/workflows/workflow_FedoraJava.xml
rm -f $RPM_BUILD_ROOT/%{_sysconfdir}/libreport/workflows.d/report_fedora_java.conf
rm -f $RPM_BUILD_ROOT/%{_sysconfdir}/libreport/events.d/java_event_fedora.conf
rm -f $RPM_BUILD_ROOT%{_mandir}/man5/report_fedora_java.conf.5
rm -f $RPM_BUILD_ROOT%{_mandir}/man5/java_event_fedora.conf.5
%endif

# Remove unwanted RHEL specific workflow configuration files
%if 0%{!?rhel:1}
rm -f $RPM_BUILD_ROOT/%{_datadir}/libreport/workflows/workflow_RHELJava.xml
rm -f $RPM_BUILD_ROOT/%{_sysconfdir}/libreport/workflows.d/report_rhel_java.conf
rm -f $RPM_BUILD_ROOT/%{_sysconfdir}/libreport/events.d/java_event_rhel.conf
rm -f $RPM_BUILD_ROOT%{_mandir}/man5/report_rhel_java.conf.5
rm -f $RPM_BUILD_ROOT%{_mandir}/man5/java_event_rhel.conf.5
%endif


%files
%doc LICENSE README AUTHORS
%config(noreplace) %{_sysconfdir}/libreport/plugins/bugzilla_format_java.conf
%config(noreplace) %{_sysconfdir}/libreport/plugins/bugzilla_formatdup_java.conf
%config(noreplace) %{_sysconfdir}/libreport/events.d/java_event.conf
%{_mandir}/man5/java_event.conf.5*
%{_mandir}/man5/bugzilla_format_java.conf.5*
%{_mandir}/man5/bugzilla_formatdup_java.conf.5*

%if 0%{?fedora}
%{_datadir}/libreport/workflows/workflow_FedoraJava.xml
%config(noreplace) %{_sysconfdir}/libreport/events.d/java_event_fedora.conf
%config(noreplace) %{_sysconfdir}/libreport/workflows.d/report_fedora_java.conf
%{_mandir}/man5/java_event_fedora.conf.5*
%{_mandir}/man5/report_fedora_java.conf.5*
%endif

%if 0%{?rhel}
%{_datadir}/libreport/workflows/workflow_RHELJava.xml
%config(noreplace) %{_sysconfdir}/libreport/events.d/java_event_rhel.conf
%config(noreplace) %{_sysconfdir}/libreport/workflows.d/report_rhel_java.conf
%{_mandir}/man5/java_event_rhel.conf.5*
%{_mandir}/man5/report_rhel_java.conf.5*
%endif

# install only unversioned shared object because the package is a Java plugin
# and not a system library but unfortunately the library must be placed in ld
# library paths
%{_libdir}/lib%{name}.so


%check
make test


%post -p /sbin/ldconfig


%postun -p /sbin/ldconfig



%changelog
* Thu Jan 09 2014 Jakub Filak <jfilak@redhat.com> - 1.0.6-1.rhbz1051198.1
- Catch all exceptions thrown by JNI functions
- Related: #1051198

* Tue Oct 01 2013 Jakub Filak <jfilak@redhat.com> - 1.0.6-1
- Fix a deadlock in GC start callback
- Disable experimental features in production releases

* Tue Jul 30 2013 Jakub Filak <jfilak@redhat.com> - 1.0.5-1
- Provide a proper configuration for libreport

* Thu Jul 18 2013 Jakub Filak <jfilak@redhat.com> - 1.0.4-1
- Stop creating an empty log file

* Tue Jul 16 2013 Jakub Filak <jfilak@redhat.com> - 1.0.3-1
- Fix tests on arm

* Tue Jul 09 2013 Jakub Filak <jfilak@redhat.com> - 1.0.2-1
- Do not crash on empty command line options

* Mon Jul 08 2013 Jakub Filak <jfilak@redhat.com> - 1.0.1-1
- Fix tests on ppc and s390 on both 32 and 64 bit

* Thu Jun 27 2013 Jakub Filak <jfilak@redhat.com> - 1.0.0-1
- Publicly releasable version

* Mon Jun 03 2013 Jakub Filak <jfilak@redhat.com> - 0.1.2-1
- Start versioning library
- Drop build dependency on abrt-devel

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
