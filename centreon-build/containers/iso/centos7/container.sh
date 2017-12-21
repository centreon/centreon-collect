#!/bin/sh

set -e
set -x

# ----------------------------------
# Download minimal Centos 7 and .rpm
# ----------------------------------

# Download CentOS 7 iso, Anaconda and Dependencies.
wget http://srvi-repo.int.centreon.com/iso/CentOS-7-x86_64-Minimal-1708.iso
wget http://yum.centreon.com/standard/3.4/el7/stable/noarch/RPMS/centreon-release-3.4-4.el7.centos.noarch.rpm
yum -y --disablerepo=updates install --nogpgcheck centreon-release-3.4-4.el7.centos.noarch.rpm

# Create mount point and tree.
rm -rf mount centreon-iso
mkdir -p mount

# -----------
# Extract ISO
# -----------

# Mount the downloaded ISO file and copy the files
mount -t iso9660 -o loop CentOS-7-x86_64-Minimal-1708.iso mount/
cp -Rp mount centreon-iso
umount mount

# -----------------------------------------
# Download packages for basic configuration
# -----------------------------------------

# Retrieve the necessary packages.
yum -y --disablerepo=updates install yum-utils
yum -y --disablerepo=updates install --nogpgcheck --downloadonly --downloaddir=centreon-iso/Packages/ centreon-base-config-centreon-engine centreon centreon-widget-* MariaDB-server centreon-poller-centreon-engine

# Unpack the addon Anaconda Centreon and create the file "product.img"
cd /tmp/addon
find . | cpio -c -o | gzip -9 > ../product.img
cd -
mv -f /tmp/product.img centreon-iso/images/

# Add the packages present in the minimum ISO Centos 7 and the "comps.xml" file
cp centreon-iso/repodata/*-c7-minimal-x86_64-comps.xml centreon-iso/c7-minimal-x86_64-comps.xml

# Create the repository
yum -y --disablerepo=updates install createrepo
createrepo -g centreon-iso/c7-minimal-x86_64-comps.xml centreon-iso/

# ----------------------------
# Generate Custom Centreon ISO
# ----------------------------

cd centreon-iso
genisoimage -input-charset utf-8 -U -r -v -T -J -joliet-long -V "CentOS 7 x86_64" -volset "CentOS 7 x86_64" -A "CentOS 7 x86_64" -b isolinux/isolinux.bin -c isolinux/boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table -eltorito-alt-boot -e images/efiboot.img -no-emul-boot -o /tmp/ces.iso .
implantisomd5 --force /tmp/ces.iso
