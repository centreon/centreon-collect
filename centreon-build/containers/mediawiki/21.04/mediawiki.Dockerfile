# Base information.
FROM mediawiki:1.31
LABEL maintainer="Matthieu Kermagoret <mkermagoret@centreon.com>"

RUN mkdir -p -m 755 /var/www/data/locks
COPY mediawiki/21.04/my_wiki.sqlite /var/www/data/
COPY mediawiki/21.04/my_wiki_l10n_cache.sqlite /var/www/data/
COPY mediawiki/21.04/wikicache.sqlite /var/www/data/
RUN chown -R www-data:www-data /var/www/data
COPY mediawiki/21.04/LocalSettings.php /var/www/html/
COPY mediawiki/21.04/php.ini /usr/local/etc/php/conf.d/centreon.ini
