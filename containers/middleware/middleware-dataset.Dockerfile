# Base information.
FROM ci.int.centreon.com:5000/mon-middleware:latest
LABEL maintainer="Kevin Duret <kduret@centreon.com>"

ENV DATAPATH middleware/data

COPY middleware/install-middleware-dataset.sh /tmp/
COPY $DATAPATH/company.sql $DATAPATH/subscription.sql $DATAPATH/instance.sql /usr/local/src/

RUN chmod +x /tmp/install-middleware-dataset.sh && \
    /tmp/install-middleware-dataset.sh
