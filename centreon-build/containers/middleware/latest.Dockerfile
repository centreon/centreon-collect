# Base information.
FROM ubuntu:16.04
LABEL maintainer="Matthieu Kermagoret <mkermagoret@centreon.com>"

# Install dependencies.
ENV DEBIAN_FRONTEND noninteractive
COPY middleware/mysql_pubkey.asc /usr/local/src/mysql_pubkey.asc
RUN apt-get update && \
    apt-get install -y debconf-utils wget lsb-release awscli && \
    gpg --import /usr/local/src/mysql_pubkey.asc && \
    apt-key add /usr/local/src/mysql_pubkey.asc && \
    wget -q https://dev.mysql.com/get/mysql-apt-config_0.8.14-1_all.deb && \
    echo 'mysql-apt-config mysql-apt-config/select-server select mysql-5.7' | debconf-set-selections && \
    echo 'mysql-apt-config mysql-apt-config/select-product select Ok' | debconf-set-selections && \
    echo 'mysql-community-server mysql-community-server/root-pass password centreon' | debconf-set-selections && \
    echo 'mysql-community-server mysql-community-server/re-root-pass password centreon' | debconf-set-selections && \
    dpkg -i mysql-apt-config_0.8.14-1_all.deb && \
    apt-get update && \
    apt-get install --allow-unauthenticated -y build-essential curl \
        mysql-client mysql-community-server mysql-server \
        netcat php-cli php-curl php-mysql unicode-data

# By default MySQL listens only to the loopback interface.
RUN sed -i s/127.0.0.1/0.0.0.0/g /etc/mysql/mysql.conf.d/mysqld.cnf

# Install Node.js and NPM.
RUN curl -sL https://deb.nodesource.com/setup_8.x | bash - && \
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

# Configure aws unknown credentials
RUN aws configure set aws_access_key_id default_access_key && \
    aws configure set aws_secret_access_key default_secret_key

# Entry point.
COPY middleware/run.sh /usr/local/bin/container.sh
ENTRYPOINT ["/usr/local/bin/container.sh"]
