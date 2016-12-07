# Base information.
FROM osixia/openldap:latest
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

ENV LDAP_ORGANISATION Centreon
ENV LDAP_DOMAIN centreon.com
ENV LDAP_ADMIN_PASSWORD centreon
ENV LDAP_TLS false

COPY openldap/conf /tmp/conf

CMD ldapadd -x -D "cn=admin,dc=centreon,dc=com" -w centreon -f /tmp/conf/ou.ldif
CMD ldapadd -x -D "cn=admin,dc=centreon,dc=com" -w centreon -f /tmp/conf/user.ldif
CMD ldapadd -x -D "cn=admin,dc=centreon,dc=com" -w centreon -f /tmp/conf/group.ldif

