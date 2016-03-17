FROM alanfranz/drb-epel-7-x86-64
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>
COPY ces-standard-testing.centos7.repo /etc/yum.repos.d/ces-standard-testing.repo
RUN mkdir /usr/share/monitoring
COPY build-dependencies.centos7.txt /usr/share/monitoring/build-dependencies.txt
RUN yum install --downloadonly `cat /usr/share/monitoring/build-dependencies.txt`
# Workaround, yum does not seem to exit correctly.
RUN rm -f /var/run/yum.pid
