# Base information.
FROM ubuntu:latest
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Dependencies.
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update
RUN apt-get install -y lftp rinse yum createrepo mkisofs isomd5sum python sudo xz-utils

# Scripts.
COPY iso/3.4-4/container.sh /usr/local/bin/container.sh
