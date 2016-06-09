#!/bin/sh

clean_repository() {
  cd $1
  rpms="centreon
        centreon-base-config-centreon-engine
        centreon-common
        centreon-installed
        centreon-perl-libs
        centreon-plugin-meta
        centreon-plugins
        centreon-poller-centreon-engine
        centreon-trap
        centreon-web
        centreon-broker
        centreon-broker-cbd
        centreon-broker-cbmod
        centreon-broker-core
        centreon-broker-debuginfo
        centreon-broker-devel
        centreon-broker-graphite
        centreon-broker-influxdb
        centreon-broker-storage
        centreon-engine
        centreon-engine-bench
        centreon-engine-daemon
        centreon-engine-debuginfo
        centreon-engine-devel
        centreon-engine-extcommands"
  for rpm in $rpms ; do
    old=`ls | grep '^'$rpm'-[0-9]\.[0-9]\.[0-9]-[0-9]\+\.[0-9a-f]\+.el[67].\(centos.\)\(noarch\|i386\|x86_64\)\.rpm' | head -n -1`
    if [ -n "$old" ] ; then
      for to_delete in $old ; do
        echo "REMOVING $to_delete"
        rm -f "$to_delete"
      done
    fi
  done
}

clean_repository /srv/repos/standard/3/unstable/noarch/RPMS
clean_repository /srv/repos/standard/3/unstable/i386/RPMS
clean_repository /srv/repos/standard/3/unstable/x86_64/RPMS
clean_repository /srv/repos/standard/4/unstable/noarch/RPMS
clean_repository /srv/repos/standard/4/unstable/x86_64/RPMS
