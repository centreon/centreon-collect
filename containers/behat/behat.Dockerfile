# Base.
FROM centos:6
MAINTAINER Alexandre Fouille <afouille@centreon.com>
RUN echo 'gpgcheck=0' >> /etc/yum.conf
RUN echo 'http_caching=none' >> /etc/yum.conf

# Install properly packaged dependencies.
RUN mkdir /usr/share/monitoring
RUN yum install --nogpgcheck -y php-cli php-mbstring wget gcc bzip2 make curl perl
RUN curl -fsSL https://get.docker.com/ | sh
RUN wget ftp://ftp.gnu.org/gnu/parallel/parallel-latest.tar.bz2
RUN tar xvf parallel-latest.tar.bz2
RUN cd parallel* && ./configure && make && make install
