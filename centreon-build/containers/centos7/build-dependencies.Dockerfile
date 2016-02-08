FROM alanfranz/drb-epel-7-x86-64
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>
# RUN yum install wget
# RUN wget http://yum.centreon.com/standard/3.0/stable/noarch/RPMS/ces-release-3.0-1.noarch.rpm
# RUN yum install ces-release-3.0-1.noarch.rpm
RUN mkdir /usr/share/monitoring
COPY build-dependencies.txt /usr/share/monitoring/build-dependencies.txt
RUN yum install --downloadonly `cat /usr/share/monitoring/build-dependencies.txt`
