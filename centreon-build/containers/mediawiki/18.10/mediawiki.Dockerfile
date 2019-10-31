# Base information.
FROM mediawiki:1.31
LABEL maintainer="Matthieu Kermagoret <mkermagoret@centreon.com>"

RUN mkdir -p -m 755 /var/www/data/locks
COPY mediawiki/18.10/my_wiki.sqlite /var/www/data/
COPY mediawiki/18.10/wikicache.sqlite /var/www/data/
RUN chown -R www-data:www-data /var/www/data
COPY mediawiki/18.10/LocalSettings.php /var/www/html/
