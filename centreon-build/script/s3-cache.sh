#!/bin/sh

set -e
set -x

if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <composer|npm>"
  exit 1
fi
COMPONENT="$1"

BUILD_IMAGE="registry.centreon.com/mon-build-dependencies-21.10:centos7"

SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
SCP_PREFIX="scp -o StrictHostKeyChecking=no"

S3_DIRECTORY=s3://centreon-build/jenkins-cache

if [ "$COMPONENT" = "composer" ]; then
  PKG_SUM=$(md5sum composer.json | cut -d\  -f 1)-$(md5sum composer.lock | cut -d\  -f 1)
  DEPS_DIRECTORY=vendor
elif [ "$COMPONENT" = "npm" ]; then
  PKG_SUM=$(md5sum package.json | cut -d\  -f 1)-$(md5sum package-lock.json | cut -d\  -f 1)
  DEPS_DIRECTORY=node_modules
fi

DEPS_TARBALL="${DEPS_DIRECTORY}.tar.gz"

rm -rf $DEPS_TARBALL

foundDirectory=$($SSH_REPO "aws s3 ls ${S3_DIRECTORY}/ | grep ${PKG_SUM} | wc -c")
if [ "${foundDirectory}" -eq "0" ]; then
  rm -rf $DEPS_DIRECTORY
  docker pull "$BUILD_IMAGE"
  if [ "$COMPONENT" = "composer" ]; then
    containerid=`docker create $BUILD_IMAGE /bin/bash -c "cd /usr/local/src && tar zxf $PROJECT-$VERSION.tar.gz && cd $PROJECT-$VERSION && composer install && tar zcf vendor.tar.gz vendor"`
  elif [ "$COMPONENT" = "npm" ]; then
    containerid=`docker create $BUILD_IMAGE /bin/bash -c "cd /usr/local/src && tar zxf $PROJECT-$VERSION.tar.gz && cd $PROJECT-$VERSION && npm ci && tar zcf node_modules.tar.gz node_modules"`
  fi

  docker cp "../$PROJECT-$VERSION.tar.gz" "$containerid:/usr/local/src/"
  docker start -a "$containerid"
  docker cp "$containerid:/usr/local/src/$PROJECT-$VERSION/$DEPS_TARBALL" "$DEPS_TARBALL"
  docker stop "$containerid"
  docker rm "$containerid"

  $SCP_PREFIX $DEPS_TARBALL ubuntu@srvi-repo.int.centreon.com:/tmp/${DEPS_TARBALL}
  $SSH_REPO "aws s3 cp /tmp/${DEPS_TARBALL} ${S3_DIRECTORY}/${PKG_SUM}/${DEPS_TARBALL}"
else
  $SSH_REPO "aws s3 cp ${S3_DIRECTORY}/${PKG_SUM}/${DEPS_TARBALL} /tmp/${DEPS_TARBALL}"
  $SCP_PREFIX "ubuntu@srvi-repo.int.centreon.com:/tmp/${DEPS_TARBALL}" "${DEPS_TARBALL}"
  tar xzf ${DEPS_TARBALL}
fi
