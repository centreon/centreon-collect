# Base information.
FROM centos:latest
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Dependencies.
ENV DEBIAN_FRONTEND noninteractive
RUN yum update -y
RUN yum install -y lftp rinse wget openssh-clients unzip gzip yum-utils createrepo mkisofs isomd5sum python sudo xz-utils

# Scripts.
COPY iso/3.4-4/container.sh /usr/local/bin/container.sh
