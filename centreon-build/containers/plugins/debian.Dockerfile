FROM ci.int.centreon.com:5000/mon-build-dependencies:debian9-armhf
LABEL maintainer="Kevin Duret <kduret@centreon.com>"

RUN apt-get install -y git \
  openssh-client \
  cpanminus \
  libjson-perl \
  libfile-copy-recursive-perl
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
ENTRYPOINT ["/usr/bin/perl", "/script/centreon_plugins_packaging.pl", "--no-rm-builddir", "--package-type=deb"]
