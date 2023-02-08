##
## Copyright 2022 Centreon
##

Summary: Centreon collect's softwares collection
Name: centreon-collect
Version: %{VERSION}
Release: %{RELEASE}%{?dist}
License:        ASL 2.0
Source: %{name}-%{version}.tar.gz
Source1: centreonengine_integrate_centreon_engine2centreon.sh

%define thismajor 23.04.0
%define nextmajor 23.10.0

Group: Applications/Communications
URL: https://github.com/centreon/centreon-collect.git
Packager: David Boucher <dboucher@centreon.com>
Vendor: Centreon Entreprise Server (CES) Repository, http://yum.centreon.com/standard/

%description
Centreon Collect is a software collection containing centreon-broker,
centreon-engine, centreon-clib and centreon-connectors.

BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root

BuildRequires: cmake3 >= 3.15
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: gnutls-devel >= 3.3.29
BuildRequires: libgcrypt-devel
BuildRequires: lua-devel
BuildRequires: make
BuildRequires: perl
BuildRequires: perl-ExtUtils-Embed
BuildRequires: perl-devel
BuildRequires: rrdtool-devel
BuildRequires: systemd
Requires: centreon-clib = %{version}-%{release}
Requires: centreon-broker-core = %{version}-%{release}
Requires: centreon-engine = %{version}-%{release}
Requires: centreon-connector = %{version}-%{release}

%package -n centreon-clib
Summary: Centreon core library.
Group: Development/Libraries

%description -n centreon-clib
Centreon Clib is a common library for all Centreon
products written in C/C++.

%package -n centreon-clib-devel
Summary: Provide include files for Centreon Clib.
Group: Development/Libraries

%description -n centreon-clib-devel
Centreon Clib devel provide include files to build
Centreon products written in C/C++.


%package -n centreon-engine
Summary: Centreon Engine monitoring core.
Group: Applications/System
Requires: centreon-engine-daemon = %{version}-%{release}
Requires: centreon-engine-extcommands = %{version}-%{release}

%description -n centreon-engine
Centreon Engine is a monitoring engine, compatible with Nagios
configuration, designed to monitor hosts and services on your network.


%package -n centreon-engine-daemon
Summary: Centreon Engine Daemon is the daemon to schedule checks.
Group: Application/System
Requires: centreon-clib = %{version}-%{release}
%{?systemd_requires}

%description -n centreon-engine-daemon
Centreon Engine is a monitoring engine that schedule checks on your
network services and hosts.


%package -n centreon-engine-extcommands
Summary: Centreon Engine External Commands allow to other applications to send command into the daemon.
Group: Application/System
Requires: centreon-engine-daemon = %{version}-%{release}

%description -n centreon-engine-extcommands
Centreon Engine External Commands allow to other applications to send
command into the daemon. External applications can submit commands by
writing to the command file, which is periodically processed by the
engine daemon.


%package -n centreon-engine-devel
Summary: Provide include files for Centreon Engine.
Group: Application/System
Requires: centreon-clib-devel = %{version}-%{release}

%description -n centreon-engine-devel
Centreon Engine devel provide include files to develop Centreon Engine
Modules or Centreon Engine Connector.


%package -n centreon-engine-bench
Summary: Centreon Engine benchmarking tools.
Group: Application/System
Requires: centreon-clib = %{version}-%{release}

%description -n centreon-engine-bench
Some Centreon Engine benchmarking tools.


%package -n centreon-connector
Summary: Centreon Connector provide some tools for Centreon Engine to monitoring and management system.
Group: Application/System
Requires: centreon-connector-perl = %{version}-%{release}
Requires: centreon-connector-ssh = %{version}-%{release}

%description -n centreon-connector
Centreon Connector provide a monitoring tools, compatible with
Centreon-Engine configuration, designed to monitor and manage system.


%package -n centreon-connector-perl
Summary: Centreon Connector Perl provide embedded perl for Centreon-Engine.
Group: Application/System
Requires: centreon-clib = %{version}-%{release}
Requires: perl

%description -n centreon-connector-perl
Centreon Connector Perl provide embedded perl for Centreon Engine
a monitoring engine.


%package -n centreon-connector-ssh
Summary: Centreon Connector SSH provide persistante connection between checks.
Group: Application/System
Requires: centreon-clib = %{version}-%{release}
Requires: libgcrypt

%description -n centreon-connector-ssh
Centreon Connector SSH provide persistante connection between checks.


%package -n centreon-broker
Summary: Store Centreon Engine/Nagios events in a database.
Group: Applications/Communications
Requires: centreon-common >= %{thismajor}
Requires: centreon-common < %{nextmajor}
Requires: coreutils

%description -n centreon-broker
Centreon Broker is a Centreon Engine/Nagios module that report events in
one or multiple databases.


%package -n centreon-broker-core
Summary: Centreon Broker's shared library.
Group: Applications/Communications
Requires: gnutls >= 3.3.29
Requires: lua
Requires: centreon-broker = %{version}-%{release}
Requires: centreon-broker-storage = %{version}-%{release}

%description -n centreon-broker-core
Centreon core holds Centreon Broker's default modules;


%package -n centreon-broker-storage
Summary: Centreon Broker's shared library for prefdata storage.
Group: Applications/Communications
Requires: centreon-broker-core = %{version}-%{release}

%description -n centreon-broker-storage
storage holds Centreon Broker's prefdata storage.


%package -n centreon-broker-graphite
Summary: Write Centreon performance data to Graphite.
Group: Applications/Communications
Requires: centreon-broker-core = %{version}-%{release}

%description -n centreon-broker-graphite
This module of Centreon Broker allows you to write performance data
generated by plugins (run themselves by Centreon Engine) to a Graphite
database.


%package -n centreon-broker-influxdb
Summary: Write Centreon performance data to InfluxDB.
Group: Applications/Communications
Requires: centreon-broker-core = %{version}-%{release}

%description -n centreon-broker-influxdb
This module of Centreon Broker allows you to write performance data
generated by plugins (run themselves by Centreon Engine) to a Graphite
database.


%package -n centreon-broker-cbd
Summary: Centreon Broker daemon.
Group: System Environment/Daemons
Requires: centreon-broker-core = %{version}-%{release}
%{?systemd_requires}

%description -n centreon-broker-cbd
The Centreon Broker daemon can aggregates output of multiple cbmod and store
events in a DB from a single point.

%package -n centreon-broker-cbmod
Summary: Centreon Broker as Centreon Engine 2 module.
Group: Applications/Communications
Requires: centreon-broker-core = %{version}-%{release}
Requires: centreon-engine = %{version}-%{release}

%description -n centreon-broker-cbmod
This module can be loaded by Centreon Engine.


%package -n centreon-broker-devel
Summary: Centreon Broker devel libraries.
Group: Applications/Communications

%description -n centreon-broker-devel
Include files needed to develop a module Centreon Broker.


%package -n centreon-collect-client
Summary: Centreon Collect gRPC Client. It can be used to exchange with cbd or centengine
Group: Applications/Communications
Requires: centreon-broker-core = %{version}-%{release}
Requires: centreon-engine = %{version}-%{release}

%description -n centreon-collect-client
This software is a gRPC client designed to easily send commands to cbd or
centengine.


%prep
%setup -q -n %{name}-%{version}

%build
pip3 install conan --upgrade
conan install . -s compiler.cppstd=14 -s compiler.libcxx=libstdc++11 --build=missing

cmake3 \
        -G "Ninja" \
        -DWITH_TESTING=0 \
        -DWITH_BENCH=1 \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DWITH_ENGINE_LOGROTATE_SCRIPT=1 \
        -DWITH_STARTUP_DIR=%{_unitdir} \
        -DWITH_STARTUP_SCRIPT=systemd \
        -DWITH_USER_BROKER=centreon-broker \
        -DWITH_GROUP_BROKER=centreon-broker \
        -DWITH_USER_ENGINE=centreon-engine \
        -DWITH_GROUP_ENGINE=centreon-engine \
        -DWITH_DAEMONS=y \
        -DWITH_CONFIG_FILES=y \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        .

#%{__make} --build .
ninja -j 8

%install
%{__rm} -rf $RPM_BUILD_ROOT
%{__install} -d $RPM_BUILD_ROOT%{_sbindir}
%{__install} -d -m 0775 $RPM_BUILD_ROOT%{_localstatedir}/log/centreon-broker
%{__install} -d -m 0775 $RPM_BUILD_ROOT%{_localstatedir}/lib/centreon-broker
%{__install} -d -m 0775 $RPM_BUILD_ROOT%{_sysconfdir}/centreon-broker
%{__install} -d $RPM_BUILD_ROOT%{_unitdir}
%{__install} -d $RPM_BUILD_ROOT%{_sysconfdir}/logrotate.d
%{__install} -d $RPM_BUILD_ROOT%{_sysconfdir}/centreon-engine/conf.d
%{__install} -d $RPM_BUILD_ROOT%{_datadir}/doc/centreon-broker
%{__install} -d $RPM_BUILD_ROOT%{_datadir}/centreon-broker/lua
%{__install} -m 644 broker/script/centreon-broker.logrotate $RPM_BUILD_ROOT%{_sysconfdir}/logrotate.d/cbd
%{__install} -d $RPM_BUILD_ROOT%{_localstatedir}/log/centreon-engine
%{__install} -d $RPM_BUILD_ROOT%{_localstatedir}/log/centreon-engine/archives
%{__install} -d $RPM_BUILD_ROOT%{_localstatedir}/lib/centreon-engine
%{__install} -d $RPM_BUILD_ROOT%{_localstatedir}/lib/centreon-engine/rw
touch $RPM_BUILD_ROOT%{_localstatedir}/log/centreon-engine/centengine.debug
%{__install} -d $RPM_BUILD_ROOT%{_datadir}/centreon-engine/extra
%{__cp} %SOURCE1 $RPM_BUILD_ROOT%{_datadir}/centreon-engine/extra/integrate_centreon_engine2centreon.sh

DESTDIR="$RPM_BUILD_ROOT" ninja install

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%pre -n centreon-broker
%{_bindir}/getent group centreon-broker &>/dev/null || %{_sbindir}/groupadd -r centreon-broker 2> /dev/null || :
%{_bindir}/getent passwd centreon-broker &>/dev/null || %{_sbindir}/useradd -m -g centreon-broker -d %{_localstatedir}/lib/centreon-broker -r centreon-broker 2> /dev/null || :
if id centreon &>/dev/null; then
  %{_sbindir}/usermod -a -G centreon-broker centreon
fi
if id centreon-gorgone &>/dev/null; then
  %{_sbindir}/usermod -a -G centreon-gorgone centreon-broker
fi
if id centreon-engine &>/dev/null; then
  %{_sbindir}/usermod -a -G centreon-broker centreon-engine
  %{_sbindir}/usermod -a -G centreon-engine centreon-broker
fi
if id nagios &>/dev/null; then
  %{_sbindir}/usermod -a -G centreon-broker nagios
fi

%pre -n centreon-engine-daemon
if ! id centreon-engine &>/dev/null; then
    %{_sbindir}/useradd -d %{_localstatedir}/lib/centreon-engine -r centreon-engine &>/dev/null
fi
if id centreon-broker &>/dev/null; then
    %{_sbindir}/usermod -a -G centreon-engine centreon-broker
fi
if id centreon-gorgone &>/dev/null; then
    %{_sbindir}/usermod -a -G centreon-gorgone centreon-engine
fi
%define httpgroup apache
if id -g %{httpgroup} &>/dev/null; then
    %{_sbindir}/usermod -a -G centreon-engine %{httpgroup}
fi
if id -g nagios &>/dev/null; then
    %{_sbindir}/usermod -a -G centreon-engine nagios
fi

%post -n centreon-engine-daemon
%systemd_post centengine.service || :

%preun -n centreon-engine-daemon
%systemd_preun centengine.service || :


%post -n centreon-broker
chown -R centreon-broker:centreon-broker /var/lib/centreon-broker
chmod -R g+w /var/lib/centreon-broker
chown -R centreon-broker:centreon-broker /var/log/centreon-broker
chmod -R g+w /var/log/centreon-broker

%post -n centreon-broker-cbd
%systemd_post cbd.service || :

%pre -n centreon-broker-cbd
# Stop cbd daemons for compatibility
if [ -f /etc/init.d/cbd-central-broker ]; then
	/etc/init.d/cbd-central-broker stop &>/dev/null || :
fi

%preun -n centreon-broker-cbd
%systemd_preun cbd.service || :

%files -n centreon-clib
%defattr(-,root,root,-)
%{_libdir}/libcentreon_clib.so
%doc clib/LICENSE

%files -n centreon-clib-devel
%defattr(-,root,root,-)
%{_includedir}/centreon-clib
%doc clib/LICENSE

%files -n centreon-engine

%files -n centreon-engine-daemon
%defattr(-,centreon-engine,centreon-engine,-)
%attr(0664,centreon-engine,centreon-engine) %config(noreplace) %{_sysconfdir}/centreon-engine/centengine.cfg
%attr(0664,centreon-engine,centreon-engine) %config(noreplace) %{_sysconfdir}/centreon-engine/resource.cfg
%attr(0664,centreon-engine,centreon-engine) %config(noreplace) %{_sysconfdir}/centreon-engine/commands.cfg
%attr(0664,centreon-engine,centreon-engine) %config(noreplace) %{_sysconfdir}/centreon-engine/timeperiods.cfg

%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/logrotate.d/centengine
%attr(755, root, root) %{_unitdir}/centengine.service
%{_sbindir}/centengine
%{_sbindir}/centenginestats
%attr(0775,root,root) %{_datadir}/centreon-engine/extra/integrate_centreon_engine2centreon.sh
%attr(0755,centreon-engine,centreon-engine) %{_localstatedir}/log/centreon-engine/
%attr(0755,centreon-engine,centreon-engine) %dir %{_localstatedir}/lib/centreon-engine/
%doc engine/license.txt

%files -n centreon-engine-extcommands
%defattr(-,root,root,-)
%{_libdir}/centreon-engine/externalcmd.so
%attr(0775,centreon-engine,centreon-engine) %{_localstatedir}/lib/centreon-engine/rw
%doc engine/license.txt

%files -n centreon-engine-devel
%defattr(-,root,root,-)
%{_includedir}/centreon-engine
%doc engine/license.txt

%files -n centreon-engine-bench
%defattr(-,root,root,-)
%{_libdir}/centreon-engine/bench_passive_module.so
%{_sbindir}/centengine_bench_passive

%files -n centreon-connector

%files -n centreon-connector-perl
%attr(0775,root,root) %{_libdir}/centreon-connector/centreon_connector_perl

%files -n centreon-connector-ssh
%attr(0775,root,root) %{_libdir}/centreon-connector/centreon_connector_ssh

%files -n centreon-broker
%defattr(-,centreon-broker,centreon-broker,-)
%{_localstatedir}/log/centreon-broker
%{_localstatedir}/lib/centreon-broker
%dir %{_sysconfdir}/centreon-broker

%files -n centreon-broker-storage
%defattr(-,root,root,-)
%{_datadir}/centreon/lib/centreon-broker/20-storage.so
%{_datadir}/centreon/lib/centreon-broker/20-unified_sql.so
%{_datadir}/centreon/lib/centreon-broker/70-rrd.so

%files -n centreon-broker-graphite
%defattr(-,root,root,-)
%{_datadir}/centreon/lib/centreon-broker/70-graphite.so

%files -n centreon-broker-influxdb
%defattr(-,root,root,-)
%{_datadir}/centreon/lib/centreon-broker/70-influxdb.so

%files -n centreon-broker-cbd
%defattr(-,centreon-broker,centreon-broker,-)
%attr(0664,centreon-broker,centreon-broker) %config(noreplace) %{_sysconfdir}/centreon-broker/central-broker.json
%attr(0664,centreon-broker,centreon-broker) %config(noreplace) %{_sysconfdir}/centreon-broker/central-rrd.json
%attr(0664,centreon-broker,centreon-broker) %config(noreplace) %{_sysconfdir}/centreon-broker/watchdog.json
%defattr(-,root,root,-)
%{_sbindir}/cbd
%{_sbindir}/cbwd

%defattr(-,root,root,-)
%{_datadir}/doc/centreon-broker/

%attr(755, root, root) %{_unitdir}/cbd.service

%files -n centreon-broker-core
%defattr(-,root,root,-)
%{_datadir}/centreon/lib/centreon-broker/10-neb.so
%{_datadir}/centreon/lib/centreon-broker/15-stats.so
%{_datadir}/centreon/lib/centreon-broker/20-bam.so
%{_datadir}/centreon/lib/centreon-broker/50-tcp.so
%{_datadir}/centreon/lib/centreon-broker/50-grpc.so
%{_datadir}/centreon/lib/centreon-broker/60-tls.so
%{_datadir}/centreon/lib/centreon-broker/70-lua.so
%{_datadir}/centreon/lib/centreon-broker/80-sql.so
%{_sysconfdir}/logrotate.d/cbd
%defattr(0775,centreon-broker,centreon-broker,-)
%{_datadir}/centreon-broker
%{_datadir}/centreon-broker/lua

%files -n centreon-broker-cbmod
%defattr(-,centreon-broker,centreon-broker,-)
%attr(0664,centreon-broker,centreon-broker) %config(noreplace) %{_sysconfdir}/centreon-broker/central-module.json
%defattr(-,root,root,-)
%{_libdir}/nagios/cbmod.so

%post -n centreon-broker-cbmod
%{_bindir}/getent passwd centreon-engine &>/dev/null && %{_sbindir}/usermod -a -G centreon-broker centreon-engine
%{_bindir}/getent group centreon-engine &>/dev/null && %{_sbindir}/usermod -a -G centreon-engine centreon-broker

%files -n centreon-broker-devel
%defattr(-,root,root,-)
%{_prefix}/include/centreon-broker
%doc broker/LICENSE
%{_includedir}/centreon-broker

%files -n centreon-collect-client
%defattr(-,root,root,-)
%{_bindir}/ccc

%files
%{_exec_prefix}/lib/systemd/system/cbd.service
%{_exec_prefix}/lib/systemd/system/centengine.service
%{_localstatedir}/log/centreon-engine/centengine.debug
%{_localstatedir}/log/centreon-engine/centengine.log
%{_localstatedir}/log/centreon-engine/retention.dat
%{_localstatedir}/log/centreon-engine/status.dat


%changelog
* Fri Dec 3 2021 David Boucher <dboucher@centreon.com> 22.04.0-1
- First version of this spec file.
