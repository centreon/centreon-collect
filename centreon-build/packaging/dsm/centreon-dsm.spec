%define debug_package %{nil}
Name:           centreon-dsm
Version:        2.2.0
Release:        3%{?dist}
Summary:        Centreon-dsm add-on for Centreon
Group:          System Environment/Base
License:        GPLv2
URL:            http://forge.centreon.com/projects/centreon-dsm
Source0:        %{name}-%{version}.tar.gz
Source1:        centreon-macroreplacement.txt
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildArch:      noarch
BuildRequires:  centreon-devel
Requires:       centreon
Requires:	centreon-dsm-client, centreon-dsm-server

%description
This add-on is built to manage passive alerts into slots of alerts. Alerts are not defined
into the configuration. This module give possibility to collect all alerts into a tray of
events.

%package server
Summary:        Centreon-dsm server
Group:          Networking/Other
Requires(post): /sbin/chkconfig
Requires:	centreon

%description server
Centreon-dsm server package

%package client
Summary:	Centreon-dsm client
Group:		Networking/Other

%description client
Centreon-dsm client package

######################################################
# Prepare the build
######################################################
%prep
%setup -q

# Change macro
find . -type f | xargs sed -i -f %{SOURCE1}
find .          \
        -type f \
        -exec %{__grep} -qE '(@DB_CENTSTORAGE@)' {} ';'   \
        -exec %{__sed} -i -e 's|@DB_CENTSTORAGE@|'"centreon_storage"'|g'\
	-exec %{__grep} -qE '(@CENTREON_BINDIR@)' {} ';'   \
	-exec %{__sed} -i -e 's|@CENTREON_BINDIR@|'"/usr/share/centreon/bin/"'|g'\
        {} ';'

%build

%install
rm -rf $RPM_BUILD_ROOT
# Install centreon-dsm README
%{__install} -d $RPM_BUILD_ROOT%{centreon_www}/modules/
%{__install} -d $RPM_BUILD_ROOT%{centreon_www}/modules/%{name}
%{__cp} README $RPM_BUILD_ROOT%{centreon_www}/modules/%{name}/README

# Install centreon-dsm web files
%{__cp} -rp www/modules/%{name} $RPM_BUILD_ROOT%{centreon_www}/modules/

# Install centreon-dsm bin
%{__install} -d -m 0775 %buildroot/usr/share/centreon/bin
%{__install} -m 0755 bin/* %buildroot/usr/share/centreon/bin

# Install daemon starting script
%{__install} -d -m 0755 %buildroot%{_sysconfdir}/init.d
%{__install} -m 0755 libinstall/init.d.dsmd %buildroot%{_sysconfdir}/init.d/dsmd


%clean
rm -rf $RPM_BUILD_ROOT


######################################################
# Package centreon-dsm
######################################################
%files


######################################################
# Package centreon-dsm-server
######################################################
%files server

%defattr(-,root,root,-)
# %doc www/modules/%{name}/CHANGELOG

%defattr(-,apache,apache,-)
%{centreon_www}/modules/%{name}
%{centreon_www}/modules/%{name}/README

%defattr(-,centreon,centreon,0755)
%{_datadir}/centreon/bin/dsmd.pl

%defattr(0755,root,root,-)
%{_sysconfdir}/init.d/dsmd

%post server
/sbin/chkconfig --add dsmd
/sbin/chkconfig --level 345 dsmd on
service dsmd start > /dev/null 2>&1 || echo


######################################################
# Package centreon-dsm-client
######################################################
%files client

%defattr(-,centreon,centreon,0755)
%{_datadir}/centreon/bin/dsmclient.pl

%changelog
