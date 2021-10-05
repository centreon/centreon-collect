#!/bin/sh

set -e
set -x

# ---------------
# Check arguments
# ---------------

VERSION="$1"
if [ "$VERSION" '!=' '20.04' -a "$VERSION" '!=' '20.10' -a "$VERSION" '!=' '21.04' -a "$VERSION" '!=' '21.10' ] ; then
  echo "Unsupported version $VERSION"
  exit 1
fi

# -----------
# Extract ISO
# -----------

# Download minimal CentOS image.
wget http://srvi-repo.int.centreon.com/iso/CentOS-7-x86_64-Minimal-1908.iso

# Create mount point and tree.
rm -rf mount centreon-iso
mkdir -p mount

# Mount the downloaded ISO file and copy the files
mount -t iso9660 -o loop CentOS-7-x86_64-Minimal-1908.iso mount/
cp -Rp mount centreon-iso
umount mount


# -------------------------------------------------------------
# Download and install repositories (scl, epel, remi, Centreon)
# -------------------------------------------------------------

yum install -y yum-utils centos-release-scl

yum -y --disablerepo=updates install --nogpgcheck --downloadonly --downloaddir=centreon-iso/Packages/ centos-release-scl
yum -y --disablerepo=updates install centos-release-scl

if [ "$VERSION" = '20.04' ] ; then
  yum install -y https://yum.centreon.com/standard/20.04/el7/stable/noarch/RPMS/centreon-release-20.04-1.el7.centos.noarch.rpm
elif [ "$VERSION" = '20.10' ] ; then
  yum install -y https://yum.centreon.com/standard/20.10/el7/stable/noarch/RPMS/centreon-release-20.10-2.el7.centos.noarch.rpm
elif [ "$VERSION" = '21.04' ] ; then
  yum install -y https://yum.centreon.com/standard/21.04/el7/stable/noarch/RPMS/centreon-release-21.04-4.el7.centos.noarch.rpm
else
  yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
  yum install -y https://rpms.remirepo.net/enterprise/remi-release-7.rpm
  yum-config-manager --enable remi-php80
  yum install -y https://yum.centreon.com/standard/21.10/el7/stable/noarch/RPMS/centreon-release-21.10-1.el7.centos.noarch.rpm
fi

# -----------------------------------------
# Download packages for basic configuration
# -----------------------------------------

# Retrieve the necessary packages.
yum -y --disablerepo=updates --enablerepo='centreon-testing*' install --nogpgcheck --downloadonly --downloaddir=centreon-iso/Packages/ centreon-base-config-centreon-engine centreon 'centreon-widget-*' mariadb-server centreon-poller-centreon-engine

# Unpack the addon Anaconda Centreon and create the file "product.img"
cd /tmp/addon
find . | cpio -c -o | gzip -9 > ../product.img
cd -
mv -f /tmp/product.img centreon-iso/images/

# Add the packages present in the minimum ISO Centos 7 and the "comps.xml" file
cp centreon-iso/repodata/*-c7-minimal-x86_64-comps.xml centreon-iso/c7-minimal-x86_64-comps.xml

# Create the repository
yum -y --disablerepo=updates install createrepo
createrepo -g c7-minimal-x86_64-comps.xml centreon-iso/

# ----------------------------
# Generate Custom Centreon ISO
# ----------------------------

cd centreon-iso
genisoimage -input-charset utf-8 -U -r -v -T -J -joliet-long -V "CentOS 7 x86_64" -volset "CentOS 7 x86_64" -A "CentOS 7 x86_64" -b isolinux/isolinux.bin -c isolinux/boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table -eltorito-alt-boot -e images/efiboot.img -no-emul-boot -o /tmp/ces.iso .
implantisomd5 --force /tmp/ces.iso
