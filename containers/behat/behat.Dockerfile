# Base.
FROM centos:6
MAINTAINER Alexandre Fouille <afouille@centreon.com>
RUN echo 'gpgcheck=0' >> /etc/yum.conf
RUN echo 'http_caching=none' >> /etc/yum.conf

# Install properly packaged dependencies.
RUN mkdir /usr/share/monitoring
RUN yum install --nogpgcheck -y php-cli php-mbstring php-xml wget gcc bzip2 make curl perl git

# Init php timezone
RUN echo 'date.timezone = Europe/Paris' > /etc/php.d/centreon.ini

# Install Parallel
RUN curl -fsSL https://get.docker.com/ | sh
RUN wget ftp://ftp.gnu.org/gnu/parallel/parallel-latest.tar.bz2
RUN tar xvf parallel-latest.tar.bz2
RUN cd parallel* && ./configure && make && make install

# Install Composer
RUN curl -sS https://getcomposer.org/installer | php
RUN mv composer.phar /usr/local/bin/composer
RUN chmod +x /usr/local/bin/composer

# Install Docker
RUN curl -L https://github.com/docker/compose/releases/download/1.7.0/docker-compose-`uname -s`-`uname -m` > /usr/local/bin/docker-compose
RUN chmod +x /usr/local/bin/docker-compose
