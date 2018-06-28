# Base information.
FROM centos:7
MAINTAINER Matthieu Kermagoret <mkermagoret@centreon.com>

# Install Centreon repository.
RUN yum install -y curl && \
    curl -o centreon-release.rpm "http://yum.centreon.com/standard/3.4/el7/stable/noarch/RPMS/centreon-release-3.4-4.el7.centos.noarch.rpm" && \
    yum install -y --nogpgcheck centreon-release.rpm

# Install httpd
RUN yum install -y httpd

# Install MariaDB.
RUN yum install -y mariadb-server mariadb initscripts

# Install PHP
RUN yum install -y php php-mysql

# Install PHP deps
RUN yum install -y php-xml php-intl php-gd php-xcache

# Install mediawiki
COPY mediawiki/mediawiki.tar.gz mediawiki/LocalSettings.php /tmp/
RUN cd /tmp && \
    tar xvzf mediawiki.tar.gz && \
    mv mediawiki-1.21.11/* /var/www/html && \
    mv LocalSettings.php /var/www/html && \
    rm -f mediawiki.tar.gz

# Main script
COPY mediawiki/run.sh /usr/share/mediawiki.sh
COPY mediawiki/wikidb.db /usr/share/wikidb.db
RUN chmod +x /usr/share/mediawiki.sh
ENTRYPOINT ["/usr/share/mediawiki.sh"]
