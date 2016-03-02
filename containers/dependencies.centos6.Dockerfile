FROM centos:6
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>
COPY ces-standard-testing.repo /etc/yum.repos.d/ces-standard-testing.repo
RUN mkdir /usr/share/monitoring
COPY dependencies.centos6.txt /usr/share/monitoring/dependencies.txt
RUN yum install --nogpgcheck -y `cat /usr/share/monitoring/dependencies.txt`
