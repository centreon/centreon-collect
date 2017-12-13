# Base information.
FROM ubuntu:16.04
LABEL maintainer="Matthieu Kermagoret <mkermagoret@centreon.com>"

# Install dependencies.
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update && \
    apt-get install -y debconf-utils && \
    echo 'mysql-server mysql-server/root_password password centreon' | debconf-set-selections && \
    echo 'mysql-server mysql-server/root_password_again password centreon' | debconf-set-selections && \
    apt-get install -y build-essential curl mysql-client mysql-server netcat php-cli php-curl php-mysql unicode-data
# By default MySQL listens only to the loopback interface.
RUN sed -i s/127.0.0.1/0.0.0.0/g /etc/mysql/mysql.conf.d/mysqld.cnf

# Install Node.js and NPM.
RUN curl -sL https://deb.nodesource.com/setup_6.x | bash - && \
    apt-get install -y nodejs

# Install middleware.
COPY centreon-imp-portal-api /usr/local/src/centreon-imp-portal-api
COPY middleware/config.js middleware/private.asc /usr/local/src/centreon-imp-portal-api/
WORKDIR /usr/local/src/centreon-imp-portal-api
RUN npm install

# Install Plugin Pack JSON files.
COPY middleware/data/pluginpacks /usr/share/centreon-packs

# Install script.

COPY middleware/json2sql.php /usr/local/src/json2sql.php
COPY middleware/data/contact.sql  /usr/local/src/contact.sql
COPY middleware/install.sh /tmp/install.sh
RUN mkdir /usr/local/src/data && \
    chmod +x /tmp/install.sh && \
    /tmp/install.sh

# Entry point.
COPY middleware/run.sh /usr/local/bin/container.sh
ENTRYPOINT ["/usr/local/bin/container.sh"]
