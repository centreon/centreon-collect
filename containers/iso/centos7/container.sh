#!/bin/sh

set -e
set -x

# ---------------
# Check arguments
# ---------------

VERSION="$1"
if [ "$VERSION" '!=' '3.4' -a "$VERSION" '!=' '18.10' -a "$VERSION" '!=' '19.04' ] ; then
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

# ----------------------------------------
# Download and install Centreon repository
# ----------------------------------------

if [ "$VERSION" = '3.4' ] ; then
  wget -P centreon-iso/Packages http://yum.centreon.com/standard/3.4/el7/stable/noarch/RPMS/centreon-release-3.4-4.el7.centos.noarch.rpm
  yum -y --disablerepo=updates install --nogpgcheck centreon-iso/Packages/centreon-release-3.4-4.el7.centos.noarch.rpm
elif [ "$VERSION" = '18.10' ] ; then
  yum -y --disablerepo=updates install --nogpgcheck --downloadonly --downloaddir=centreon-iso/Packages/ centos-release-scl
  yum -y --disablerepo=updates install centos-release-scl
  wget -P centreon-iso/Packages http://srvi-repo.int.centreon.com/yum/standard/18.10/el7/stable/noarch/RPMS/centreon-release-18.10-2.el7.centos.noarch.rpm
  yum -y --disablerepo=updates install --nogpgcheck centreon-iso/Packages/centreon-release-18.10-2.el7.centos.noarch.rpm
  sed -i -e 's|yum.centreon.com|srvi-repo.int.centreon.com/yum|g' /etc/yum.repos.d/centreon.repo
elif [ "$VERSION" = '19.04' ] ; then
  yum -y --disablerepo=updates install --nogpgcheck --downloadonly --downloaddir=centreon-iso/Packages/ centos-release-scl
  yum -y --disablerepo=updates install centos-release-scl
  wget -P centreon-iso/Packages http://srvi-repo.int.centreon.com/yum/standard/19.04/el7/stable/noarch/RPMS/centreon-release-19.04-1.el7.centos.noarch.rpm
  yum -y --disablerepo=updates install --nogpgcheck centreon-iso/Packages/centreon-release-19.04-1.el7.centos.noarch.rpm
  sed -i -e 's|yum.centreon.com|srvi-repo.int.centreon.com/yum|g' /etc/yum.repos.d/centreon.repo
else
  yum -y --disablerepo=updates install --nogpgcheck --downloadonly --downloaddir=centreon-iso/Packages/ centos-release-scl
  yum -y --disablerepo=updates install centos-release-scl
  wget -P centreon-iso/Packages http://srvi-repo.int.centreon.com/yum/standard/19.10/el7/stable/noarch/RPMS/centreon-release-19.10-1.el7.centos.noarch.rpm
  yum -y --disablerepo=updates install --nogpgcheck centreon-iso/Packages/centreon-release-19.10-1.el7.centos.noarch.rpm
  sed -i -e 's|yum.centreon.com|srvi-repo.int.centreon.com/yum|g' /etc/yum.repos.d/centreon.repo    
fi

# -----------------------------------------
# Download packages for basic configuration
# -----------------------------------------

# Retrieve the necessary packages.
yum -y --disablerepo=updates install yum-utils
if [ "$VERSION" = '3.4' ] ; then
  yum -y --disablerepo=updates install --nogpgcheck --downloadonly --downloaddir=centreon-iso/Packages/ centreon-base-config-centreon-engine centreon centreon-widget-* MariaDB-server centreon-poller-centreon-engine
else
  yum -y --disablerepo=updates --enablerepo='centreon-testing*' install --nogpgcheck --downloadonly --downloaddir=centreon-iso/Packages/ centreon-base-config-centreon-engine centreon 'centreon-widget-*' mariadb-server centreon-poller-centreon-engine
fi

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
