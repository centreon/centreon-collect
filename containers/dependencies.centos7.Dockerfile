FROM centos:7
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>
RUN echo 'gpgcheck=0' >> /etc/yum.conf
RUN echo 'http_caching=none' >> /etc/yum.conf
COPY ces-standard-testing.centos7.repo /etc/yum.repos.d/ces-standard-testing.repo
RUN mkdir /usr/share/monitoring
COPY dependencies.centos7.txt /usr/share/monitoring/dependencies.txt
RUN yum install --nogpgcheck -y `cat /usr/share/monitoring/dependencies.txt`
RUN yum install --nogpgcheck -y git
