FROM monitoring-dependencies:centos6
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>
COPY build-dependencies.txt /usr/share/monitoring/build-dependencies.txt
RUN yum install -y `cat /usr/share/monitoring/build-dependencies.txt`
COPY unittest-broker.sh /usr/local/bin/unittest-broker
COPY unittest-engine.sh /usr/local/bin/unittest-engine
RUN chmod +x /usr/local/bin/unittest-broker /usr/local/bin/unittest-engine
