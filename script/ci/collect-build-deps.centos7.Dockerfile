FROM registry.centreon.com/centos:7

# Install Centreon repository.
RUN yum install --nogpgcheck -y http://yum.centreon.com/standard/20.04/el7/stable/noarch/RPMS/centreon-release-20.04-1.el7.centos.noarch.rpm epel-release

# Install build dependencies.
RUN yum install -y \
  cmake3 \
  gcc \
  gcc-c++ \
  gnutls-devel \
  MariaDB-devel \
  perl-ExtUtils-Embed \
  python3-pip \
  rrdtool-devel

# Install Conan.
RUN pip3 install conan && \
  conan remote add centreon-center https://api.bintray.com/conan/centreon/centreon && \
  conan remote remove conan-center && \
  cp -r /root/.conan /.conan && \
  chmod -R 777 /.conan
