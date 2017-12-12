#!/bin/sh

# Download CentOS 7 iso and Anaconda
wget http://srvi-repo.int.centreon.com/iso/CentOS-7-x86_64-Minimal-1708.iso
wget https://centos.pkgs.org/7/centos-x86_64/anaconda-widgets-devel-21.48.22.121-1.el7.centos.x86_64.rpm.html

# Create tree
mkdir mount addon centreon-iso

# Mount the downloaded ISO file and copy the files
mount -t iso9660 -o loop CentOS-7-x86_64-Minimal-1708.iso mount/
cp -Rp mount/* centreon-iso/
umount /mount

# Unpack the addon Anaconda Centreon and create the file "product.img"
cd addon
unzip com_centreon_server_role.zip
find . | cpio -c -o | gzip 9 > ../product.img
cd ..
mv -f product.img centreon-iso/images/

# Generate Custom Centreon ISO
cd centreon-iso
genisoimage -input-charset utf-8 -U -r -v -T -J -joliet-long -V "CentOS 7 x86_64" -volset
"CentOS 7 x86_64" -A "CentOS 7 x86_64" -b isolinux/isolinux.bin -c isolinux/boot.cat -no-emul-
boot -boot-load-size 4 -boot-info-table -eltorito-alt-boot -e images/efiboot.img -no-emul-boot -o
../centreon-test.iso .
cd ..
implantisomd5 --force centreon-test.iso
