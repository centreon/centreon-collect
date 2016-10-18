FROM centos:7
MAINTAINER  Maximilien Bersoult mbersoult@centreon.com

RUN yum update -y
RUN yum install -y git \
  openssh \
  perl-App-cpanminus \
  perl-JSON \
  perl-File-Copy-Recursive \
  && rm -rf /var/cache/yum/*
RUN cpanm App::FatPacker
RUN mkdir -p /script/centreon
COPY centreon /script/centreon
COPY centreon_plugins_packaging.pl /script
RUN mkdir -p /root/.ssh
ADD id_rsa /root/.ssh/id_rsa
RUN chmod 700 /root/.ssh/id_rsa
RUN echo -e "Host srvi-ces-repository.int.centreon.com\n\tStrictHostKeyChecking no\n" >> /root/.ssh/config
COPY gitconfig /root/.gitconfig
ENTRYPOINT ["/usr/bin/perl", "/script/centreon_plugins_packaging.pl"]

