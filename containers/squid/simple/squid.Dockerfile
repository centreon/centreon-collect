# Base information.
FROM ci.int.centreon.com:5000/mon-dependencies:centos7
MAINTAINER Kevin Duret <kduret@centreon.com>

# Install squid
RUN yum install -y squid
COPY squid/simple/squid.conf /etc/squid/squid.conf

# Main script
COPY squid/run.sh /tmp/squid.sh
RUN chmod +x /tmp/squid.sh
ENTRYPOINT ["/tmp/squid.sh"]
