FROM centos:6
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>
RUN yum install -y wget
RUN wget http://yum.centreon.com/standard/3.0/stable/noarch/RPMS/ces-release-3.0-1.noarch.rpm
RUN yum install --nogpgcheck -y ces-release-3.0-1.noarch.rpm
RUN mkdir /usr/share/monitoring
COPY dependencies.centos6.txt /usr/share/monitoring/dependencies.txt
RUN yum install -y `cat /usr/share/monitoring/dependencies.txt`
