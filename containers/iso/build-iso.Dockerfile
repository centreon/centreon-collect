# Base information.
FROM ubuntu:latest
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Dependencies.
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update
RUN apt-get install -y lftp rinse yum createrepo mkisofs isomd5sum python sudo xz-utils

# Scripts.
COPY iso/container.sh /usr/local/bin/container.sh
COPY iso/make-iso /usr/local/bin/make-iso
COPY iso/lib /tmp/lib
