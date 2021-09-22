# Base information.
FROM osixia/openldap:1.1.11
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

ENV LDAP_ORGANISATION Centreon
ENV LDAP_DOMAIN centreon.com
ENV LDAP_ADMIN_PASSWORD centreon
ENV LDAP_TLS false

ADD openldap/bootstrap /container/service/slapd/assets/config/bootstrap
