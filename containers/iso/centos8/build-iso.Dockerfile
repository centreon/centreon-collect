# Base information.
FROM registry.centreon.com/centos:8.2.2004
MAINTAINER Laurent Pinsivy <lpinsivy@centreon.com>

# Dependencies.
#RUN dnf install -y lftp wget openssh-clients unzip yum-utils createrepo mkisofs isomd5sum python38 sudo xz-utils
RUN dnf install -y lftp wget openssh-clients unzip yum-utils createrepo mkisofs isomd5sum python38 sudo

# Scripts.
COPY iso/centos8/container.sh /usr/local/bin/container.sh
