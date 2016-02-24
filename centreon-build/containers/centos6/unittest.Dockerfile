# Base information.
FROM monitoring-dependencies:centos6
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Install all build dependencies of Centreon components.
COPY build-dependencies.txt /usr/share/monitoring/build-dependencies.txt
RUN yum install -y `cat /usr/share/monitoring/build-dependencies.txt`

# Install unit test specific components.
RUN wget https://dl.fedoraproject.org/pub/epel/epel-release-latest-6.noarch.rpm
RUN yum install --nogpgcheck -y epel-release-latest-6.noarch.rpm
RUN yum install -y php-phpunit-PHPUnit

# Install unit tests scripts.
COPY unittest-broker.sh /usr/local/bin/unittest-broker
COPY unittest-engine.sh /usr/local/bin/unittest-engine
COPY unittest-ppe.sh /usr/local/bin/unittest-ppe
RUN chmod +x /usr/local/bin/unittest-broker /usr/local/bin/unittest-engine /usr/local/bin/unittest-ppe
