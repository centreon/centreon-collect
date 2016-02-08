FROM alanfranz/drb-epel-7-x86-64
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>
RUN yum install wget
RUN wget -O - http://yum.centreon.com/standard/4.0/testing/ces-standard-testing.repo | sed 's/gpgcheck=1/gpgcheck=0/g' > /etc/yum.repos.d/ces-standard-testing.repo
RUN mkdir /usr/share/monitoring
COPY build-dependencies.txt /usr/share/monitoring/build-dependencies.txt
RUN yum install --downloadonly `cat /usr/share/monitoring/build-dependencies.txt`
