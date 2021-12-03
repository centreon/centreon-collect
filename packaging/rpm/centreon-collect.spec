##
## Copyright 2021 Centreon
##

Summary: Centreon collect's softwares collection
Name: centreon-collect
Version: %{VERSION}
Release: %{RELEASE}%{?dist}
License:        ASL 2.0
Source: %{name}-%{version}.tar.gz
%define thismajor 22.04.0
%define nextmajor 22.05.0
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
BuildRequires: lua-devel
BuildRequires: libgcrypt-devel
BuildRequires: rrdtool-devel
BuildRequires: systemd
BuildRequires: gnutls-devel >= 3.3.29
BuildRequires: perl
BuildRequires: perl-devel
BuildRequires: perl-ExtUtils-Embed
Requires: centreon-clib = %{version}-%{release}
Requires: centreon-broker-core = %{version}-%{release}

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
Requires: libssh2 >= 1.4
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


%prep
%setup -q -n %{name}-%{version}

%build
pip3 install conan --upgrade
conan install . -s compiler.libcxx=libstdc++11 --build=missing

cmake3 \
        -DWITH_TESTING=0 \
        -DWITH_BENCH=1 \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DWITH_ENGINE_LOGROTATE_SCRIPT=1 \
        -DWITH_PREFIX_CONF_BROKER=%{_sysconfdir}/centreon-broker \
        -DWITH_PREFIX_INC=%{_includedir}/centreon-collect \
        -DWITH_PREFIX_MODULES=%{_datadir}/centreon/lib/centreon-broker \
        -DWITH_PREFIX_LIB_BROKER=%{_libdir}/nagios/ \
        -DWITH_PREFIX_VAR=%{_localstatedir}/lib/centreon-broker \
        -DWITH_STARTUP_DIR=%{_unitdir} \
        -DWITH_STARTUP_SCRIPT=systemd \
        -DWITH_USER_BROKER=centreon-broker \
        -DWITH_GROUP_BROKER=centreon-broker \
        -DWITH_DAEMONS='central-rrd;central-broker' \
        -DWITH_CONFIG_FILES=y \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        .
%{__make} -j5 %{?_smp_mflags}
#%{__make} -j9 %{?_smp_mflags} VERBOSE="1"

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
%{__install} -m 644 centreon-broker/script/centreon-broker.logrotate $RPM_BUILD_ROOT%{_sysconfdir}/logrotate.d/cbd
%{__make} install DESTDIR="$RPM_BUILD_ROOT"

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%pre
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
if ! id %{user} &>/dev/null; then
    %{_sbindir}/useradd -d %{_localstatedir}/lib/centreon-engine -r %{user} &>/dev/null
fi
if id centreon-broker &>/dev/null; then
    %{_sbindir}/usermod -a -G %{user} centreon-broker
fi
if id centreon-gorgone &>/dev/null; then
    %{_sbindir}/usermod -a -G centreon-gorgone %{user}
fi
%define httpgroup apache
if id -g %{httpgroup} &>/dev/null; then
    %{_sbindir}/usermod -a -G %{user} %{httpgroup}
fi
if id -g nagios &>/dev/null; then
    %{_sbindir}/usermod -a -G %{user} nagios
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
%doc centreon-clib/LICENSE

%files -n centreon-clib-devel
%defattr(-,root,root,-)
%{_includedir}/centreon-clib
%doc centreon-clib/LICENSE

%files -n centreon-engine-daemon
%defattr(-,centreon-engine,centreon-engine,-)
%attr(0664,%{user},%{user}) %config(noreplace) %{_sysconfdir}/centreon-engine/centengine.cfg
%attr(0664,%{user},%{user}) %config(noreplace) %{_sysconfdir}/centreon-engine/resource.cfg
%attr(0664,%{user},%{user}) %config(noreplace) %{_sysconfdir}/centreon-engine/objects/*.cfg

%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/logrotate.d/centengine
%{_unitdir}/centengine.service
%{_sbindir}/centengine
%{_sbindir}/centenginestats
#%attr(0775,root,root) %{_datadir}/centreon-engine/extra/integrate_centreon_engine2centreon.sh
%attr(0755,%{user},%{user}) %{_localstatedir}/log/centreon-engine/
%attr(0755,%{user},%{user}) %dir %{_localstatedir}/lib/centreon-engine/
%doc centreon-engine/license.txt

%files -n centreon-engine-extcommands
%defattr(-,root,root,-)
%{_libdir}/centreon-engine/externalcmd.so
%attr(0775,%{user},%{user}) %{_localstatedir}/lib/centreon-engine/rw
%doc centreon-engine/license.txt

%files -n centreon-engine-devel
%defattr(-,root,root,-)
%{_includedir}/centreon-engine
%doc centreon-engine/license.txt

%files -n centreon-engine-bench
%defattr(-,root,root,-)
%{_libdir}/centreon-engine/bench_passive_module.so
%{_sbindir}/centengine_bench_passive

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
%defattr(664,centreon-broker,centreon-broker,-)
%config(noreplace) %{_sysconfdir}/centreon-broker/central-broker.json
%config(noreplace) %{_sysconfdir}/centreon-broker/central-rrd.json
%config(noreplace) %{_sysconfdir}/centreon-broker/watchdog.json
%defattr(-,root,root,-)
%{_sbindir}/cbd
%{_sbindir}/cbwd
#%{_sysconfdir}/centreon-broker/central-broker.json
#%{_sysconfdir}/centreon-broker/central-rrd.json
#%{_sysconfdir}/centreon-broker/watchdog.json

%defattr(-,root,root,-)
%{_datadir}/doc/centreon-broker/

%attr(755, root, root) %{_unitdir}/cbd.service

%files -n centreon-broker-core
%defattr(-,root,root,-)
%{_datadir}/centreon/lib/centreon-broker/10-neb.so
%{_datadir}/centreon/lib/centreon-broker/15-stats.so
%{_datadir}/centreon/lib/centreon-broker/20-bam.so
%{_datadir}/centreon/lib/centreon-broker/50-tcp.so
%{_datadir}/centreon/lib/centreon-broker/60-tls.so
%{_datadir}/centreon/lib/centreon-broker/70-lua.so
%{_datadir}/centreon/lib/centreon-broker/80-sql.so
%{_sysconfdir}/logrotate.d/cbd
#%defattr(0775,centreon-broker,centreon-broker,-)
#%{_datadir}/centreon-broker
#%{_datadir}/centreon-broker/lua

%files -n centreon-broker-cbmod
%defattr(664,centreon-broker,centreon-broker,-)
%config(noreplace) %{_sysconfdir}/centreon-broker/poller-module.json
%defattr(-,root,root,-)
%{_libdir}/nagios/cbmod.so
#%{_sysconfdir}/centreon-broker/poller-module.json

%post -n centreon-broker-cbmod
%{_bindir}/getent passwd centreon-engine &>/dev/null && %{_sbindir}/usermod -a -G centreon-broker centreon-engine
%{_bindir}/getent group centreon-engine &>/dev/null && %{_sbindir}/usermod -a -G centreon-engine centreon-broker

%files -n centreon-broker-devel
%defattr(-,root,root,-)
%{_prefix}/include/centreon-broker
%doc centreon-broker/LICENSE
%{_includedir}/centreon-broker

%files
%{_sysconfdir}/logrotate.d/cbd
%{_libdir}/centreon-connector/centreon_connector_perl
%{_libdir}/centreon-connector/centreon_connector_ssh
#%{_includedir}/centreon-engine/com/centreon/engine/anomalydetection.hh
#%{_includedir}/centreon-engine/com/centreon/engine/broker.hh
#%{_includedir}/centreon-engine/com/centreon/engine/broker/compatibility.hh
#%{_includedir}/centreon-engine/com/centreon/engine/broker/handle.hh
#%{_includedir}/centreon-engine/com/centreon/engine/broker/loader.hh
#%{_includedir}/centreon-engine/com/centreon/engine/check_result.hh
#%{_includedir}/centreon-engine/com/centreon/engine/checkable.hh
#%{_includedir}/centreon-engine/com/centreon/engine/checks/checker.hh
#%{_includedir}/centreon-engine/com/centreon/engine/checks/stats.hh
#%{_includedir}/centreon-engine/com/centreon/engine/circular_buffer.hh
#%{_includedir}/centreon-engine/com/centreon/engine/command_manager.hh
#%{_includedir}/centreon-engine/com/centreon/engine/commands/command.hh
#%{_includedir}/centreon-engine/com/centreon/engine/commands/command_listener.hh
#%{_includedir}/centreon-engine/com/centreon/engine/commands/connector.hh
#%{_includedir}/centreon-engine/com/centreon/engine/commands/environment.hh
#%{_includedir}/centreon-engine/com/centreon/engine/commands/forward.hh
#%{_includedir}/centreon-engine/com/centreon/engine/commands/raw.hh
#%{_includedir}/centreon-engine/com/centreon/engine/commands/result.hh
#%{_includedir}/centreon-engine/com/centreon/engine/comment.hh
#%{_includedir}/centreon-engine/com/centreon/engine/common.hh
#%{_includedir}/centreon-engine/com/centreon/engine/config.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/anomalydetection.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/anomalydetection.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/command.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/connector.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/contact.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/contactgroup.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/difference.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/globals.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/host.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/hostdependency.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/hostescalation.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/hostgroup.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/logging.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/macros.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/scheduler.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/service.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/servicedependency.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/serviceescalation.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/servicegroup.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/state.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/applier/timeperiod.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/command.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/connector.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/contact.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/contactgroup.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/daterange.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/file_info.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/group.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/host.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/hostdependency.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/hostescalation.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/hostextinfo.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/hostgroup.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/object.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/parser.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/point_2d.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/point_3d.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/service.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/servicedependency.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/serviceescalation.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/serviceextinfo.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/servicegroup.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/state.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/timeperiod.hh
#%{_includedir}/centreon-engine/com/centreon/engine/configuration/timerange.hh
#%{_includedir}/centreon-engine/com/centreon/engine/contact.hh
#%{_includedir}/centreon-engine/com/centreon/engine/contactgroup.hh
#%{_includedir}/centreon-engine/com/centreon/engine/customvariable.hh
#%{_includedir}/centreon-engine/com/centreon/engine/daterange.hh
#%{_includedir}/centreon-engine/com/centreon/engine/deleter/listmember.hh
#%{_includedir}/centreon-engine/com/centreon/engine/deleter/serviceescalation.hh
#%{_includedir}/centreon-engine/com/centreon/engine/dependency.hh
#%{_includedir}/centreon-engine/com/centreon/engine/diagnostic.hh
#%{_includedir}/centreon-engine/com/centreon/engine/downtimes/downtime.hh
#%{_includedir}/centreon-engine/com/centreon/engine/downtimes/downtime_finder.hh
#%{_includedir}/centreon-engine/com/centreon/engine/downtimes/downtime_manager.hh
#%{_includedir}/centreon-engine/com/centreon/engine/downtimes/host_downtime.hh
#%{_includedir}/centreon-engine/com/centreon/engine/downtimes/service_downtime.hh
#%{_includedir}/centreon-engine/com/centreon/engine/engine_impl.hh
#%{_includedir}/centreon-engine/com/centreon/engine/enginerpc.hh
#%{_includedir}/centreon-engine/com/centreon/engine/escalation.hh
#%{_includedir}/centreon-engine/com/centreon/engine/events/loop.hh
#%{_includedir}/centreon-engine/com/centreon/engine/events/sched_info.hh
#%{_includedir}/centreon-engine/com/centreon/engine/events/timed_event.hh
#%{_includedir}/centreon-engine/com/centreon/engine/exceptions/error.hh
#%{_includedir}/centreon-engine/com/centreon/engine/flapping.hh
#%{_includedir}/centreon-engine/com/centreon/engine/globals.hh
#%{_includedir}/centreon-engine/com/centreon/engine/hash.hh
#%{_includedir}/centreon-engine/com/centreon/engine/host.hh
#%{_includedir}/centreon-engine/com/centreon/engine/hostdependency.hh
#%{_includedir}/centreon-engine/com/centreon/engine/hostescalation.hh
#%{_includedir}/centreon-engine/com/centreon/engine/hostgroup.hh
#%{_includedir}/centreon-engine/com/centreon/engine/logging.hh
#%{_includedir}/centreon-engine/com/centreon/engine/logging/broker.hh
#%{_includedir}/centreon-engine/com/centreon/engine/logging/debug_file.hh
#%{_includedir}/centreon-engine/com/centreon/engine/logging/logger.hh
#%{_includedir}/centreon-engine/com/centreon/engine/macros.hh
#%{_includedir}/centreon-engine/com/centreon/engine/macros/clear_host.hh
#%{_includedir}/centreon-engine/com/centreon/engine/macros/clear_hostgroup.hh
#%{_includedir}/centreon-engine/com/centreon/engine/macros/clear_service.hh
#%{_includedir}/centreon-engine/com/centreon/engine/macros/clear_servicegroup.hh
#%{_includedir}/centreon-engine/com/centreon/engine/macros/defines.hh
#%{_includedir}/centreon-engine/com/centreon/engine/macros/grab.hh
#%{_includedir}/centreon-engine/com/centreon/engine/macros/grab_host.hh
#%{_includedir}/centreon-engine/com/centreon/engine/macros/grab_service.hh
#%{_includedir}/centreon-engine/com/centreon/engine/macros/grab_value.hh
#%{_includedir}/centreon-engine/com/centreon/engine/macros/misc.hh
#%{_includedir}/centreon-engine/com/centreon/engine/macros/process.hh
#%{_includedir}/centreon-engine/com/centreon/engine/my_lock.hh
#%{_includedir}/centreon-engine/com/centreon/engine/namespace.hh
#%{_includedir}/centreon-engine/com/centreon/engine/nebcallbacks.hh
#%{_includedir}/centreon-engine/com/centreon/engine/neberrors.hh
#%{_includedir}/centreon-engine/com/centreon/engine/nebmods.hh
#%{_includedir}/centreon-engine/com/centreon/engine/nebmodules.hh
#%{_includedir}/centreon-engine/com/centreon/engine/nebstructs.hh
#%{_includedir}/centreon-engine/com/centreon/engine/notification.hh
#%{_includedir}/centreon-engine/com/centreon/engine/notifier.hh
#%{_includedir}/centreon-engine/com/centreon/engine/objects.hh
#%{_includedir}/centreon-engine/com/centreon/engine/opt.hh
#%{_includedir}/centreon-engine/com/centreon/engine/restart_stats.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/applier/comment.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/applier/contact.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/applier/downtime.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/applier/host.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/applier/program.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/applier/service.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/applier/state.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/applier/utils.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/comment.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/contact.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/downtime.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/dump.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/host.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/info.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/object.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/parser.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/program.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/service.hh
#%{_includedir}/centreon-engine/com/centreon/engine/retention/state.hh
#%{_includedir}/centreon-engine/com/centreon/engine/sehandlers.hh
#%{_includedir}/centreon-engine/com/centreon/engine/service.hh
#%{_includedir}/centreon-engine/com/centreon/engine/servicedependency.hh
#%{_includedir}/centreon-engine/com/centreon/engine/serviceescalation.hh
#%{_includedir}/centreon-engine/com/centreon/engine/servicegroup.hh
#%{_includedir}/centreon-engine/com/centreon/engine/shared.hh
#%{_includedir}/centreon-engine/com/centreon/engine/statistics.hh
#%{_includedir}/centreon-engine/com/centreon/engine/statusdata.hh
#%{_includedir}/centreon-engine/com/centreon/engine/string.hh
#%{_includedir}/centreon-engine/com/centreon/engine/timeperiod.hh
#%{_includedir}/centreon-engine/com/centreon/engine/timerange.hh
#%{_includedir}/centreon-engine/com/centreon/engine/timezone_locker.hh
#%{_includedir}/centreon-engine/com/centreon/engine/timezone_manager.hh
#%{_includedir}/centreon-engine/com/centreon/engine/utils.hh
#%{_includedir}/centreon-engine/com/centreon/engine/version.hh
#%{_includedir}/centreon-engine/com/centreon/engine/xpddefault.hh
#%{_includedir}/centreon-engine/com/centreon/engine/xsddefault.hh
#%{_includedir}/centreon-engine/compatibility/broker.h
#%{_includedir}/centreon-engine/compatibility/comments.h
#%{_includedir}/centreon-engine/compatibility/common.h
#%{_includedir}/centreon-engine/compatibility/config.h
#%{_includedir}/centreon-engine/compatibility/downtime.h
#%{_includedir}/centreon-engine/compatibility/embedded_perl.h
#%{_includedir}/centreon-engine/compatibility/epn_nagios.h
#%{_includedir}/centreon-engine/compatibility/events.h
#%{_includedir}/centreon-engine/compatibility/globals.h
#%{_includedir}/centreon-engine/compatibility/locations.h
#%{_includedir}/centreon-engine/compatibility/logging.h
#%{_includedir}/centreon-engine/compatibility/macros.h
#%{_includedir}/centreon-engine/compatibility/mmap.h
#%{_includedir}/centreon-engine/compatibility/nagios.h
#%{_includedir}/centreon-engine/compatibility/nebcallbacks.h
#%{_includedir}/centreon-engine/compatibility/neberrors.h
#%{_includedir}/centreon-engine/compatibility/nebmods.h
#%{_includedir}/centreon-engine/compatibility/nebmodules.h
#%{_includedir}/centreon-engine/compatibility/nebstructs.h
#%{_includedir}/centreon-engine/compatibility/netutils.h
#%{_includedir}/centreon-engine/compatibility/objects.h
#%{_includedir}/centreon-engine/compatibility/perfdata.h
#%{_includedir}/centreon-engine/compatibility/protoapi.h
#%{_includedir}/centreon-engine/compatibility/shared.h
#%{_includedir}/centreon-engine/compatibility/sighandlers.h
#%{_includedir}/centreon-engine/compatibility/sretention.h
#%{_includedir}/centreon-engine/compatibility/statusdata.h
#%{_includedir}/centreon-engine/compatibility/utils.h
#%{_includedir}/centreon-engine/compatibility/xcddefault.h
#%{_includedir}/centreon-engine/compatibility/xodtemplate.h
#%{_includedir}/centreon-engine/compatibility/xpddefault.h
#%{_includedir}/centreon-engine/compatibility/xrddefault.h
#%{_includedir}/centreon-engine/compatibility/xsddefault.h
%{_libdir}/centreon-engine/externalcmd.so
%{_exec_prefix}/lib/systemd/system/cbd.service
%{_exec_prefix}/lib/systemd/system/centengine.service
%{_sbindir}/centengine
%{_sbindir}/centenginestats
%{_localstatedir}/log/centreon-engine/centengine.debug
%{_localstatedir}/log/centreon-engine/centengine.log
%{_localstatedir}/log/centreon-engine/retention.dat
%{_localstatedir}/log/centreon-engine/status.dat


%changelog
* Tue Nov 29 2016 Matthieu Kermagoret <mkermagoret@centreon.com> 3.0.2-3
- Fix downtime inheritance in BAM module.
