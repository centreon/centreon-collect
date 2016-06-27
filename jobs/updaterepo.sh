#!/bin/sh

#
# Update repo with new RPMs.
#

PID_FILE="/tmp/updaterepo.pid"
REPO="$1"

if [ -n "$REPO" ] ; then
  sucess=0
  while [ "$sucess" = 0 ] ; do
    if [ -f "$PID_FILE" ] ; then
      concurrent=`cat $PID_FILE | head -n 1`
      while [ -d "/proc/$concurrent" ] ; do
        sleep 1
      done
    else
      echo $$ >> "$PID_FILE"
      concurrent=`cat $PID_FILE | head -n 1`
      if [ "$concurrent" = $$ ] ; then
        createrepo "/srv/repos/$REPO"
        rm -f "$PID_FILE"
        sucess=1
      fi
    fi
  done
fi

#
# Clean the repositories.
#

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
        centreon-license-manager
        centreon-pp-manager
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
    old=`ls | grep '^'$rpm'-[0-9]\.[0-9]\.[0-9]-[0-9]\+\.[0-9a-f]\+.el[67].[centos.]*\(noarch\|i386\|x86_64\)\.rpm' | head -n -1`
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
clean_repository /srv/repos/centreon-bam/3/unstable/noarch/RPMS
clean_repository /srv/repos/centreon-bam/4/unstable/noarch/RPMS

#
# Clean after ourselves.
#
rm -f "$0"
