# Base information.
FROM centos:6
MAINTAINER Alexandre Fouille <afouille@centreon.com>

# Install httpd
RUN yum install -y httpd

# Install MySQL
RUN yum install -y mysql-server mysql

# Install PHP
RUN yum install -y php php-mysql

# Install PHP deps
RUN yum install -y php-xml php-intl php-gd texlive epel-release php-xcache

# Install mediawiki
#RUN curl -O https://releases.wikimedia.org/mediawiki/1.14/mediawiki-1.14.1.tar.gz
#RUN tar xvzf mediawiki-*.tar.gz
#RUN mv mediawiki-1.14.1/* /var/www/html
COPY mediawiki/mediawiki.tar.gz /var/www/html
RUN tar xvzf /var/www/html/mediawiki.tar.gz -C /var/www/html/
RUN rm /var/www/html/mediawiki.tar.gz

# Main script
COPY mediawiki/run.sh /usr/share/mediawiki.sh
COPY mediawiki/wikidb.db /usr/share/wikidb.db
RUN chmod +x /usr/share/mediawiki.sh
ENTRYPOINT ["/usr/share/mediawiki.sh"]
