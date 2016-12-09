# Base information.
FROM debian:jessie
MAINTAINER Kevin Duret <kduret@centreon.com>

# Change SSO DOMAIN here
ENV SSODOMAIN centreon.com

# Update system
RUN apt-get -y update && apt-get -y dist-upgrade

# Install LemonLDAP::NG repo
RUN apt-get -y install wget
RUN wget -O - http://lemonldap-ng.org/_media/rpm-gpg-key-ow2 | apt-key add -
COPY lemonldap/lemonldap-ng.list /etc/apt/sources.list.d/
COPY lemonldap/lmConf-2.js /var/lib/lemonldap-ng/conf/lmConf-2.js
COPY lemonldap/centreon-apache2.conf /etc/apache2/sites-available/centreon-apache2.conf

# Install LemonLDAP::NG packages
RUN apt-get -y update && apt-get -y install apache2 libapache2-mod-perl2 libapache2-mod-fcgid lemonldap-ng lemonldap-ng-fr-doc

# Change SSO Domain
RUN sed -i "s/example\.com/${SSODOMAIN}/g" /etc/lemonldap-ng/* /var/lib/lemonldap-ng/conf/lmConf-1.js /var/lib/lemonldap-ng/test/index.pl

# Enable sites
RUN a2ensite handler-apache2.conf
RUN a2ensite portal-apache2.conf
RUN a2ensite manager-apache2.conf
RUN a2ensite centreon-apache2.conf

RUN a2enmod fcgid perl alias rewrite proxy_http

# Remove cached configuration
RUN rm -rf /tmp/lemonldap-ng-config

ENTRYPOINT ["/usr/sbin/apache2ctl", "-D", "FOREGROUND"]