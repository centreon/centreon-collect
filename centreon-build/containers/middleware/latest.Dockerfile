# Base information.
FROM ubuntu:16.04
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Install dependencies.
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update
RUN apt-get install -y debconf-utils
RUN echo 'mysql-server mysql-server/root_password password centreon' | debconf-set-selections
RUN echo 'mysql-server mysql-server/root_password_again password centreon' | debconf-set-selections
RUN apt-get install -y build-essential mysql-client mysql-server netcat nodejs nodejs-legacy npm phantomjs php-cli php-curl php-mysql unicode-data

# Copy middleware sources.
COPY centreon-imp-portal-api /usr/local/src/centreon-imp-portal-api

# Install OpenLDAP.
RUN apt-get install -y screen slapd ldap-utils
#RUN echo 'olcRootPW: {SSHA}2pMsLy5/tCfxzQpBah2YWflQdzTKH0Py' >> '/etc/openldap/slapd.d/cn=config/olcDatabase={2}bdb.ldif'
#RUN sed -i 's/dc=acme/dc=centreon/g' '/etc/openldap/slapd.d/cn=config/olcDatabase={2}bdb.ldif'
#RUN sed -i 's/dc=acme/dc=centreon/g' '/etc/openldap/slapd.d/cn=config/olcDatabase={1}monitor.ldif'
#COPY middleware/ldap.ldif /tmp/ldap.ldif

# Install middleware.
WORKDIR /usr/local/src/centreon-imp-portal-api
COPY middleware/config.js /usr/local/src/centreon-imp-portal-api/config.js
COPY middleware/private.asc /usr/local/src/centreon-imp-portal-api/private.asc
RUN npm install

# Install Plugin Pack JSON files.
COPY middleware/data/*.json /etc/centreon/ppm/

# Install script.
RUN mkdir /usr/local/src/data
COPY middleware/json2sql.php /usr/local/src/json2sql.php
COPY middleware/data/contact.sql  /usr/local/src/contact.sql
COPY middleware/install.sh /tmp/install.sh
RUN chmod +x /tmp/install.sh
RUN /tmp/install.sh

# Entry point.
COPY middleware/run.sh /usr/local/bin/container.sh
ENTRYPOINT /usr/local/bin/container.sh
