# Base information.
FROM ci.int.centreon.com:5000/mon-dependencies:centos6
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Install all build dependencies of Centreon components.
COPY build-dependencies.centos6.txt /usr/share/monitoring/build-dependencies.txt
RUN yum install --nogpgcheck -y `cat /usr/share/monitoring/build-dependencies.txt`

# Install unit test specific components.
RUN yum install -y wget
RUN wget https://dl.fedoraproject.org/pub/epel/epel-release-latest-6.noarch.rpm
RUN yum install --nogpgcheck -y epel-release-latest-6.noarch.rpm
RUN yum install -y php-phpunit-PHPUnit php-phpunit-PHPUnit-MockObject
RUN yum install -y php-pecl-xdebug
RUN yum install -y curl
RUN curl -sS https://getcomposer.org/installer | php
RUN mv composer.phar /usr/local/bin/composer
RUN chmod +x /usr/local/bin/composer

# Install unit tests scripts.
COPY unittest/unittest-broker.sh /usr/local/bin/unittest-broker
COPY unittest/unittest-engine.sh /usr/local/bin/unittest-engine
COPY unittest/unittest-ppe.sh /usr/local/bin/unittest-ppe
COPY unittest/unittest-lm.sh /usr/local/bin/unittest-lm
RUN chmod +x /usr/local/bin/unittest-broker /usr/local/bin/unittest-engine /usr/local/bin/unittest-ppe /usr/local/bin/unittest-lm
