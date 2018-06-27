# Base information.
FROM ci.int.centreon.com:5000/centos:7.5.1804
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Dependencies.
RUN yum install -y --disablerepo=updates lftp wget openssh-clients unzip gzip yum-utils createrepo mkisofs isomd5sum python sudo xz-utils

# Scripts.
COPY iso/centos7/container.sh /usr/local/bin/container.sh
