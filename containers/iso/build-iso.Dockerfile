FROM ubuntu:latest
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update
RUN apt-get install -y lftp rinse yum createrepo mkisofs isomd5sum python
