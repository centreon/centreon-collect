#!/bin/sh

set -e
set -x

# Project.
PROJECT=centreon-license-manager

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
BUILD_CENTOS7=ci.int.centreon.com:5000/mon-build-dependencies-18.9:centos7
docker pull "$BUILD_CENTOS7"

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos7
mkdir output-centos7

# Get version.
cd "$PROJECT"
git checkout --detach "$COMMIT"
VERSION=`grep mod_release www/modules/$PROJECT/conf.php | cut -d '"' -f 4`
export VERSION="$VERSION"

# Create source tarball.
git archive --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../input/$PROJECT-$VERSION.tar.gz"
cd ..

# Retrieve (fake) spectemplate.
cat <<EOF > input/centreon-license-manager.spectemplate
%define debug_package %{nil}

Name:           centreon-license-manager
Version:        @VERSION@
Release:        @RELEASE@%{?dist}
Summary:        Centreon License Manager
%define thismajor 18.9.0
%define nextmajor 18.10.0

Group:          Applications/System
License:        Proprietary
URL:            https://www.centreon.com
Source0:        %{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildArch:      noarch
BuildRequires:  centreon-devel
Requires:       centreon-web >= %{thismajor}
Requires:       centreon-web < %{nextmajor}

%description
This extension manage licenses of proprietary Centreon products.

%prep

%build

%install

%clean

%changelog
EOF

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_CENTOS7" input output-centos7

# Copy sources to server.
SSH_REPO="ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com"
DESTDIR="/srv/sources/standard/testing/$PROJECT-$VERSION-$RELEASE"
$SSH_REPO mkdir "$DESTDIR"
scp -o StrictHostKeyChecking=no "input/$PROJECT-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:$DESTDIR/"

# Copy files to server.
FILES_CENTOS7='output-centos7/x86_64/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/18.9/el7/testing/x86_64/RPMS"
$SSH_REPO createrepo /srv/yum/standard/18.9/el7/testing/x86_64

# Generate doc.
SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-broker -V latest -p'"
