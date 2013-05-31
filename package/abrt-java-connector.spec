Name:		abrt-java-connector
Version:	0.1.0
Release:	3%{?dist}
Summary:	JNI Agent library converting Java exceptions to ABRT problems

Group:		system/utils
License:	GPL2+
URL:		https://github.com/jfilak/abrt-java-connector
Source0:    %{name}-%{version}.tar.bz2

BuildRequires:	cmake
BuildRequires:	abrt-devel
BuildRequires:	java-1.7.0-openjdk-devel
Requires:	abrt

%description
JNI Agent library converting Java exceptions to ABRT problems


%prep
%setup -q


%build
%cmake -DABRT:BOOL=ON .
make %{?_smp_mflags}


%install
make install DESTDIR=%{buildroot}


%files
%doc LICENSE README AUTHORS
%{_libdir}/lib%{name}.so


%changelog
* Fri May 30 2013 Jakub Filak <jfilak@redhat.com> - 0.1.0-3
- Build with the library name same as the package name

* Fri May 30 2013 Jakub Filak <jfilak@redhat.com> - 0.1.0-2
- Build with ABRT enabled

* Fri May 30 2013 Jakub Filak <jfilak@redhat.com> - 0.1.0-1
- Initial version
