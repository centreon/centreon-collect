#!/bin/sh

set -e
set -x

# Global parameters.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
BASE_DIR='/srv/yum'

# Check arguments.
if [ -z "$PROJECT" -o -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify PROJECT, VERSION and RELEASE environment variables."
  exit 1
fi

# Get RPM list.
case "$PROJECT" in
  centreon-as400-connector)
    DIR='/plugin-packs/3.4'
    ARCH='noarch'
    RPMS='centreon-connector-as400-server'
  ;;
  centreon-as400-connector-endoflife)
    DIR='/plugin-packs/3.0'
    ARCH='noarch'
    RPMS='centreon-connector-as400-server'
    EL7=no
  ;;
  centreon-as400-plugin)
    DIR='/plugin-packs/3.4'
    ARCH='x86_64'
    RPMS='ces-plugins-Operatingsystems-As400'
  ;;
  centreon-as400-plugin-endoflife)
    DIR='/plugin-packs/3.0'
    ARCH='x86_64'
    RPMS='ces-plugins-Operatingsystems-As400'
    EL7=no
  ;;
  centreon-awie)
    DIR='/standard/3.4'
    ARCH='noarch'
    RPMS='centreon-awie'
  ;;
  centreon-bam-server)
    DIR='/bam/3.4'
    ARCH='noarch'
    RPMS='centreon-bam-server'
  ;;
  centreon-bam-server-endoflife)
    DIR='/bam/3.4'
    ARCH='noarch'
    RPMS='centreon-bam-server'
    EL7=no
  ;;
  centreon-broker)
    DIR='standard/3.4'
    ARCH='x86_64'
    RPMS="centreon-broker
          centreon-broker-cbd
          centreon-broker-cbmod
          centreon-broker-core
          centreon-broker-debuginfo
          centreon-broker-devel
          centreon-broker-graphite
          centreon-broker-influxdb
          centreon-broker-storage"
  ;;
  centreon-broker-endoflife)
    DIR='/standard/3.3'
    ARCH='x86_64'
    RPMS="centreon-broker
          centreon-broker-cbd
          centreon-broker-cbmod
          centreon-broker-core
          centreon-broker-debuginfo
          centreon-broker-graphite
          centreon-broker-influxdb
          centreon-broker-storage"
    EL7=no
  ;;
  centreon-connector)
    DIR='/standard/3.4'
    ARCH='x86_64'
    RPMS="centreon-connector
          centreon-connector-debuginfo
          centreon-connector-perl
          centreon-connector-ssh"
  ;;
  centreon-engine)
    DIR='/standard/3.4'
    ARCH='x86_64'
    RPMS="centreon-engine
          centreon-engine-bench
          centreon-engine-daemon
          centreon-engine-debuginfo
          centreon-engine-devel
          centreon-engine-extcommands"
  ;;
  centreon-bi-engine)
    DIR='/mbi/3.4'
    ARCH='noarch'
    RPMS="centreon-bi-engine"
  ;;
  centreon-bi-etl)
    DIR='/mbi/3.4'
    ARCH='noarch'
    RPMS="centreon-bi-etl"
  ;;
  centreon-bi-report)
    DIR='/mbi/3.4'
    ARCH='noarch'
    RPMS="centreon-bi-report"
  ;;
  centreon-bi-reporting-server)
    DIR='/mbi/3.4'
    ARCH='noarch'
    RPMS="centreon-bi-reporting-server"
  ;;
  centreon-bi-server)
    DIR='/mbi/3.4'
    ARCH='noarch'
    RPMS="centreon-bi-server"
  ;;
  centreon-poller-display)
    DIR='/standard/3.4'
    ARCH='noarch'
    RPMS="centreon-poller-display
          centreon-poller-display-central"
  ;;
  centreon-web)
    DIR='/lts/3.4'
    ARCH='noarch'
    RPMS="centreon
          centreon-base-config-centreon-engine
          centreon-common
          centreon-lang-fr_FR
          centreon-installed
          centreon-perl-libs
          centreon-plugins
          centreon-poller-centreon-engine
          centreon-trap
          centreon-web"
    ;;
  centreon-web-endoflife)
    DIR='/standard/3.3'
    ARCH='noarch'
    RPMS="centreon
          centreon-base-config-centreon-engine
          centreon-common
          centreon-installed
          centreon-perl-libs
          centreon-plugin-meta
          centreon-plugins
          centreon-poller-centreon-engine
          centreon-trap
          centreon-web"
    EL7=no
  ;;
  centreon-map-web-client)
      DIR='/map/3.4'
      ARCH='noarch'
      RPMS="centreon-map4-web-client"
  ;;
  centreon-map-server)
      DIR='/map/3.4'
      ARCH='noarch'
      RPMS="centreon-map4-server"
  ;;
  centreon-widget-*)
    DIR='/standard/3.4'
    ARCH='noarch'
    RPMS="$PROJECT"
  ;;
esac

# Move all RPMs to stable.
for rpm in $RPMS ; do
  if [ "$EL7" '!=' 'no' ] ; then
    $SSH_REPO mv "$BASE_DIR/$DIR/el7/testing/$ARCH/RPMS/$rpm-$VERSION-$RELEASE.el7.*$ARCH.rpm" "$BASE_DIR/$DIR/el7/stable/$ARCH/RPMS"
  fi
done

# Create repo.
if [ "$EL7" '!=' 'no' ] ; then
  $SSH_REPO createrepo "$BASE_DIR/$DIR/el7/stable/$ARCH"
  $SSH_REPO createrepo "$BASE_DIR/$DIR/el7/testing/$ARCH"
fi
