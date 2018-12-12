# Base information.
FROM registry.centreon.com/mon-squid-simple:latest
MAINTAINER Kevin Duret <kduret@centreon.com>

# Install squid
COPY squid/basic-auth/squid.conf /etc/squid/squid.conf
COPY squid/basic-auth/passwords /etc/squid/passwords

# Main script
COPY squid/run.sh /tmp/squid.sh
RUN chmod +x /tmp/squid.sh
ENTRYPOINT ["/tmp/squid.sh"]
