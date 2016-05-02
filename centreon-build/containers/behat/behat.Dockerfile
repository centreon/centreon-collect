# Base.
FROM centos:7
MAINTAINER Alexandre Fouille <afouille@centreon.com>
RUN echo 'gpgcheck=0' >> /etc/yum.conf
RUN echo 'http_caching=none' >> /etc/yum.conf

# Install properly packaged dependencies.
RUN mkdir /usr/share/monitoring
RUN yum install --nogpgcheck -y php-cli  wget docker
RUN wget http://linuxsoft.cern.ch/cern/centos/7/cern/x86_64/Packages/parallel-20150522-1.el7.cern.noarch.rpm
RUN yum install --nogpgcheck -y parallel-20150522-1.el7.cern.noarch.rpm