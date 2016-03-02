# Base information.
FROM ci.int.centreon.com:5000/mon-web:centos6
MAINTAINER Alexandre Fouille <afouille@centreon.com>

# Copy LM sources.
COPY centreon-license-manager /usr/share/centreon/www/modules/centreon-license-manager

# Install the tools needed for the License Manager installation.
RUN yum install -y curl
RUN curl -sL https://rpm.nodesource.com/setup_4.x | bash -
RUN yum install -y nodejs

# Install script.
COPY lm-install.sh /tmp/lm-install.sh
RUN chmod +x /tmp/lm-install.sh
RUN /tmp/lm-install.sh
