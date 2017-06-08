FROM ci.int.centreon.com:5000/mon-middleware:latest
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# npm packages.
COPY centreon-imp-portal-api/package.json /usr/local/src/centreon-imp-portal-api/package.json
RUN npm install

# Database.
COPY centreon-imp-portal-api/database/create.sql /usr/local/src/centreon-imp-portal-api/database/create.sql
RUN /tmp/install.sh

# Static sources.
COPY middleware/config.js /usr/local/src/centreon-imp-portal-api/config.js
COPY centreon-imp-portal-api/lib /usr/local/src/centreon-imp-portal-api/lib
COPY centreon-imp-portal-api/localserver /usr/local/src/centreon-imp-portal-api/localserver
