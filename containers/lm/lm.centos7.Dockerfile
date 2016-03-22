# Base information.
FROM ci.int.centreon.com:5000/mon-web:centos7
MAINTAINER Alexandre Fouille <afouille@centreon.com>

# Copy LM sources.
COPY centreon-license-manager /usr/share/centreon/www/modules/centreon-license-manager

# Install the tools needed for the License Manager installation.
RUN yum install -y curl
RUN curl -sL https://rpm.nodesource.com/setup_4.x | bash -
RUN yum install -y nodejs

# Install script.
COPY install-centreon-module.php /tmp/install-centreon-module.php
COPY lm/lm-install-centos6.sh /tmp/lm-install-centos7.sh
RUN chmod +x /tmp/install-centreon-module.php /tmp/lm-install-centos7.sh
RUN /tmp/lm-install-centos7.sh
