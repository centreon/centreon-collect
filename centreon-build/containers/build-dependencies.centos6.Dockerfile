FROM alanfranz/drb-epel-6-x86-64
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>
COPY ces-standard-testing.repo /etc/yum.repos.d/ces-standard-testing.repo
RUN mkdir /usr/share/monitoring
COPY build-dependencies.centos6.txt /usr/share/monitoring/build-dependencies.txt
RUN yum install --downloadonly `cat /usr/share/monitoring/build-dependencies.txt`
# Workaround, yum does not seem to exit correctly.
RUN rm -f /var/run/yum.pid
