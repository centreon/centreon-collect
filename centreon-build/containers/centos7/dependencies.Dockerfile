FROM centos:7
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>
RUN yum install -y wget
RUN wget -O /etc/yum.repos.d/ces-standard-testing.repo http://yum.centreon.com/standard/4.0/testing/ces-standard-testing.repo
# Currently needed to provide nagios-plugins.
RUN wget https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
RUN yum install --nogpgcheck -y epel-release-latest-7.noarch.rpm
RUN yum clean all
RUN mkdir /usr/share/monitoring
COPY dependencies.txt /usr/share/monitoring/dependencies.txt
RUN yum install --nogpgcheck -y `cat /usr/share/monitoring/dependencies.txt`
