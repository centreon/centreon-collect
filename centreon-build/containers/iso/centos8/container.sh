#!/bin/sh

set -e
set -x

# ---------------
# Check arguments
# ---------------

VERSION="$1"
if [ "$VERSION" '!=' '20.10' ] ; then
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

# ----------------------------------------
# Download and install Centreon repository
# ----------------------------------------

yum -y install --nogpgcheck --downloadonly --downloaddir=centreon-iso/Packages/ dnf-plugins-core epel-release
yum -y install dnf-plugins-core epel-release
wget -P centreon-iso/Packages http://srvi-repo.int.centreon.com/yum/standard/20.10/el8/stable/noarch/RPMS/centreon-release-20.10-2.el8.noarch.rpm
yum -y install --nogpgcheck centreon-iso/Packages/centreon-release-20.10-2.el8.noarch.rpm
sed -i -e 's|yum.centreon.com|srvi-repo.int.centreon.com/yum|g' /etc/yum.repos.d/centreon.repo

# -----------------------------------------
# Download packages for basic configuration
# -----------------------------------------

# Retrieve the necessary packages.
yum -y install yum-utils
yum -y --enablerepo='centreon-testing*' install --nogpgcheck --downloadonly --downloaddir=centreon-iso/Packages/ centreon-base-config-centreon-engine centreon 'centreon-widget-*' mariadb-server centreon-poller-centreon-engine

# Unpack the addon Anaconda Centreon and create the file "product.img"
cd /tmp/addon
find . | cpio -c -o | gzip -9 > ../product.img
cd -
mv -f /tmp/product.img centreon-iso/images/

# Add the packages present in the minimum ISO Centos 7 and the "comps.xml" file
cp centreon-iso/repodata/*-c8-minimal-x86_64-comps.xml centreon-iso/c8-minimal-x86_64-comps.xml

# Create the repository
yum -y --disablerepo=updates install createrepo
createrepo -g c8-minimal-x86_64-comps.xml centreon-iso/

# ----------------------------
# Generate Custom Centreon ISO
# ----------------------------

cd centreon-iso
genisoimage -input-charset utf-8 -U -r -v -T -J -joliet-long -V "CentOS 8 x86_64" -volset "CentOS 8 x86_64" -A "CentOS 8 x86_64" -b isolinux/isolinux.bin -c isolinux/boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table -eltorito-alt-boot -e images/efiboot.img -no-emul-boot -o /tmp/ces.iso .
implantisomd5 --force /tmp/ces.iso
