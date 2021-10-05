#!/bin/sh

set -e
set -x

# ---------------
# Check arguments
# ---------------

VERSION="$1"
if [ "$VERSION" '!=' '20.10' -a "$VERSION" '!=' '21.04' -a "$VERSION" '!=' '21.10' ] ; then
  echo "Unsupported version $VERSION"
  exit 1
fi

# -----------
# Extract ISO
# -----------

# Download minimal CentOS image.
wget http://srvi-repo.int.centreon.com/iso/CentOS-8.2.2004-x86_64-minimal.iso

# Create mount point and tree.
rm -rf mount centreon-iso
mkdir -p mount

# Mount the downloaded ISO file and copy the files
mount -t iso9660 -o loop CentOS-8.2.2004-x86_64-minimal.iso mount/
cp -Rp mount centreon-iso
umount mount

# --------------------------------------------------------
# Download and install repositories (epel, remi, Centreon)
# --------------------------------------------------------

dnf install -y dnf-plugins-core epel-release

if [ "$VERSION" = '20.10' ] ; then
  dnf install -y https://yum.centreon.com/standard/20.10/el8/stable/noarch/RPMS/centreon-release-20.10-2.el8.noarch.rpm
elif [ "$VERSION" = '21.04' ] ; then
  dnf install -y https://yum.centreon.com/standard/21.04/el8/stable/noarch/RPMS/centreon-release-21.04-4.el8.noarch.rpm
else
  dnf install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm
  dnf install -y https://rpms.remirepo.net/enterprise/remi-release-8.rpm
  dnf config-manager --set-enabled 'powertools'
  dnf module reset php
  dnf module install php:remi-8.0
  dnf install -y https://yum.centreon.com/standard/21.10/el8/stable/noarch/RPMS/centreon-release-21.10-1.el8.noarch.rpm
fi

# -----------------------------------------
# Download packages for basic configuration
# -----------------------------------------

# Retrieve the necessary packages.
yum -y --enablerepo='centreon-testing*' install --nogpgcheck --downloadonly --downloaddir=centreon-iso/Packages/ centreon-base-config-centreon-engine centreon 'centreon-widget-*' mariadb-server centreon-poller-centreon-engine

# Unpack the addon Anaconda Centreon and create the file "product.img"
cd /tmp/addon
find . | cpio -c -o | gzip -9 > ../product.img
cd -
mv -f /tmp/product.img centreon-iso/images/

# Add the packages present in the minimum ISO Centos 7 and the "comps.xml" file
cp centreon-iso/Minimal/repodata/*-comps-Minimal.x86_64.xml centreon-iso/c7-minimal-x86_64-comps.xml

# Create the repository
yum -y --disablerepo=updates install createrepo
createrepo -g c8-comps-Minimal.x86_64.xml centreon-iso/

# ----------------------------
# Generate Custom Centreon ISO
# ----------------------------

cd centreon-iso
genisoimage -input-charset utf-8 -U -r -v -T -J -joliet-long -V "CentOS 8 x86_64" -volset "CentOS 8 x86_64" -A "CentOS 8 x86_64" -b isolinux/isolinux.bin -c isolinux/boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table -eltorito-alt-boot -e images/efiboot.img -no-emul-boot -o /tmp/ces.iso .
implantisomd5 --force /tmp/ces.iso
