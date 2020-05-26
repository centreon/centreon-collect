FROM registry.centreon.com/centos:7

# Install Centreon repository.
RUN yum install --nogpgcheck -y http://yum.centreon.com/standard/20.04/el7/stable/noarch/RPMS/centreon-release-20.04-1.el7.centos.noarch.rpm

# Install build dependencies.
RUN yum install -y \
  cmake \
  gcc \
  gcc-c++ \
  gnutls-devel \
  lua-devel \
  MariaDB-devel \
  MariaDB-shared \
  python3-pip \
  rrdtool-devel \
  systemd
RUN pip3 install conan
