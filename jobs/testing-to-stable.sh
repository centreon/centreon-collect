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
  centreon-bam-server)
    DIR='/bam/3.4'
    ARCH='noarch'
    RPMS='centreon-bam-server'
  ;;
  centreon-poller-display)
    DIR='/standard/3.4'
    ARCH='noarch'
    RPMS="centreon-poller-display
          centreon-poller-display-central"
  ;;
  centreon-web)
    DIR='/standard/3.4'
    ARCH='noarch'
    RPMS="centreon
          centreon-base-config-centreon-engine
          centreon-common
          centreon-lang-fr_FR
          centreon-installed
          centreon-perl-libs
          centreon-plugin-meta
          centreon-plugins
          centreon-poller-centreon-engine
          centreon-trap
          centreon-web"
    ;;
esac

# Move all RPMs to stable.
for rpm in $RPMS ; do
  $SSH_REPO mv "$BASE_DIR/$DIR/el6/testing/$ARCH/RPMS/$rpm-$VERSION-$RELEASE.el6.$ARCH.rpm" "$BASE_DIR/$DIR/el6/stable/$ARCH/RPMS"
  $SSH_REPO mv "$BASE_DIR/$DIR/el7/testing/$ARCH/RPMS/$rpm-$VERSION-$RELEASE.el7.centos.$ARCH.rpm" "$BASE_DIR/$DIR/el7/stable/$ARCH/RPMS"
done

# Create repo.
$SSH_REPO createrepo "$BASE_DIR/$DIR/el6/stable/$ARCH"
$SSH_REPO createrepo "$BASE_DIR/$DIR/el7/stable/$ARCH"
$SSH_REPO createrepo "$BASE_DIR/$DIR/el6/testing/$ARCH"
$SSH_REPO createrepo "$BASE_DIR/$DIR/el7/testing/$ARCH"
