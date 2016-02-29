# Base information.
FROM ci.int.centreon.com:5000/mon-lm:centos6
MAINTAINER Alexandre Fouille <afouille@centreon.com>

# Copy LM sources.
COPY centreon-license-manager /usr/share/centreon/www/modules/

# TODO

# Install script.
COPY lm/lm-install.sh /tmp/lm-install.sh
RUN chmod +x /tmp/lm-install.sh
RUN /tmp/lm-install.sh
