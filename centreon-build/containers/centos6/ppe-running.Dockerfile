# Base information.
FROM monitoring-running:centos6
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Copy PPE sources.
COPY centreon-export /usr/share/centreon/www/modules/

# Install PPE module in Centreon.
