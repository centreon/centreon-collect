FROM centos:7
LABEL maintainer="Kevin Duret <kduret@centreon.com>"

RUN yum update -y
RUN yum install -y git \
  openssh \
  perl-App-cpanminus \
  perl-JSON \
  perl-File-Copy-Recursive \
  rpm-build \
  rpm-sign \
  expect \
  && rm -rf /var/cache/yum/*
RUN cpanm App::FatPacker
RUN mkdir -p /root/.ssh
ADD plugins/id_rsa /root/.ssh/id_rsa
RUN chmod 700 /root/.ssh/id_rsa
RUN echo -e "Host srvi-repo.int.centreon.com\n\tStrictHostKeyChecking no\n" >> /root/.ssh/config
COPY plugins/gitconfig /root/.gitconfig
RUN mkdir -p /script/centreon
COPY plugins/packaging /script/packaging
COPY plugins/centreon_plugins_packaging.pl /script
COPY plugins/centreon /script/centreon
COPY ces.key /tmp/ces.key
RUN gpg --import /tmp/ces.key
ENTRYPOINT ["/usr/bin/perl", "/script/centreon_plugins_packaging.pl", "--no-rm-builddir", "--package-type=rpm"]
