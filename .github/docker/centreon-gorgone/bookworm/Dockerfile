FROM perl:slim-threaded-bookworm AS gorgone

ENV TYPE=

ARG VERSION
ARG IS_CLOUD
ARG STABILITY

ENV DEBIAN_FRONTEND=noninteractive

COPY --chmod=755 .github/docker/centreon-gorgone/bookworm/systemctl /usr/local/bin/systemctl
COPY --chmod=755 .github/docker/centreon-gorgone/bookworm/entrypoint /var/lib/centreon-gorgone/
COPY --chmod=600 .github/docker/centreon-gorgone/bookworm/centreon /etc/sudoers.d/centreon
COPY --chmod=755 .github/docker/centreon-gorgone/bookworm/centenginestats /usr/sbin/centenginestats

RUN groupadd -g 900 centreon &&\
    useradd -u 900 -g centreon -m -r -d /var/spool/centreon -c "Centreon" -s /bin/bash centreon &&\
    groupadd -g 901 centreon-engine &&\
    useradd -u 901 -g centreon-engine -m -r -d /var/lib/centreon-engine -c "Centreon Engine" -s /bin/bash centreon-engine &&\
    groupadd -g 902 centreon-broker &&\
    useradd -u 902 -g centreon-broker -m -r -d /var/lib/centreon-broker -c "Centreon Broker"  -s /bin/bash centreon-broker &&\
    groupadd -g 903 centreon-gorgone &&\
    useradd -u 903 -g centreon-gorgone -m -r -d /var/lib/centreon-gorgone -c "Centreon Gorgone"  -s /bin/bash centreon-gorgone &&\
    usermod -aG centreon centreon-gorgone && \
    usermod -aG www-data centreon-gorgone && \
    usermod -aG centreon-engine centreon-gorgone

RUN --mount=type=bind,src=packages-centreon,dst=/tmp/packages-centreon \
    --mount=type=secret,id=ARTIFACTORY_INTERNAL_REPO_USERNAME \
    --mount=type=secret,id=ARTIFACTORY_INTERNAL_REPO_PASSWORD \
    bash -e <<EOF


######################################
# additional configuration for apt   #
######################################

echo '
Acquire::Retries "10";
Acquire::https::Timeout "300";
Acquire::http::Timeout "300";
APT::Install-Recommends "false";
APT::Install-Suggests "false";
' > /etc/apt/apt.conf.d/99custom

apt-get update
apt-get install -y \
  wget \
  gnupg2 \
  ca-certificates \
  software-properties-common

VERSION_CODENAME=\$(
  . /etc/os-release
  echo \$VERSION_CODENAME
)

echo "deb https://packages.sury.org/php/ \$VERSION_CODENAME main" | tee /etc/apt/sources.list.d/sury-php.list
wget -O- https://packages.sury.org/php/apt.gpg | gpg --dearmor | tee /etc/apt/trusted.gpg.d/php.gpg  > /dev/null 2>&1

if [[ "${IS_CLOUD}" == "true" ]]; then
  echo "deb https://$(cat /run/secrets/ARTIFACTORY_INTERNAL_REPO_USERNAME):$(cat /run/secrets/ARTIFACTORY_INTERNAL_REPO_PASSWORD)@packages.centreon.com/apt-standard-internal/ \$VERSION_CODENAME-${VERSION}-unstable main" | tee -a /etc/apt/sources.list.d/centreon-unstable.list
else
  echo "deb https://packages.centreon.com/apt-standard/ \$VERSION_CODENAME-${VERSION}-stable main" | tee -a /etc/apt/sources.list.d/centreon-stable.list
  echo "deb https://packages.centreon.com/apt-standard/ \$VERSION_CODENAME-${VERSION}-testing-hotfix main" | tee -a /etc/apt/sources.list.d/centreon-testing.list
  echo "deb https://packages.centreon.com/apt-standard/ \$VERSION_CODENAME-${VERSION}-testing-release main" | tee -a /etc/apt/sources.list.d/centreon-testing.list
  echo "deb https://packages.centreon.com/apt-standard/ \$VERSION_CODENAME-${VERSION}-unstable main" | tee -a /etc/apt/sources.list.d/centreon-unstable.list
fi
echo "deb https://packages.centreon.com/apt-plugins-stable/ \$VERSION_CODENAME main" | tee -a /etc/apt/sources.list.d/centreon-plugins-stable.list
echo "deb https://packages.centreon.com/apt-plugins-testing/ \$VERSION_CODENAME main" | tee -a /etc/apt/sources.list.d/centreon-plugins-testing.list
echo "deb https://packages.centreon.com/apt-plugins-unstable/ \$VERSION_CODENAME main" | tee -a /etc/apt/sources.list.d/centreon-plugins-unstable.list
wget -O- https://packages.centreon.com/api/security/keypair/APT-GPG-KEY/public | gpg --dearmor | tee /etc/apt/trusted.gpg.d/centreon.gpg > /dev/null 2>&1

if [[ "$STABILITY" == "testing" ]]; then
  for i in \$( ls /etc/apt/sources.list.d/centreon*unstable* ); do mv \$i \$i.disabled; done
elif [[ "$STABILITY" == "stable" ]]; then
  for i in \$( ls /etc/apt/sources.list.d/centreon*{unstable,testing}* ); do mv \$i \$i.disabled; done
fi

######################################
# install and configure repositories #
######################################
apt-get update
apt-get install -y  --no-install-recommends\
        python3-minimal \
        python3.11-venv \
        libmariadb3 \
        monitoring-plugins-basic \
        sudo \
        libcryptx-perl \
        libzmq-ffi-perl \
        libev-perl \
        libyaml-libyaml-perl \
        libclass-dbi-perl \
        libdbd-sqlite3-perl \
        apt-transport-https \
        curl \
        libdbd-mysql-perl \
        libhttp-daemon-perl \
        libdigest-md5-file-perl \
        libschedule-cron-perl \
        librrds-perl \
        libhash-merge-perl \
        libnet-curl-perl \
        libcrypt-openssl-aes-perl \
        libssh-session-perl \
        libjson-xs-perl \
        libxml-simple-perl \
        iputils-ping

apt-get install -y /tmp/packages-centreon/centreon-gorgone*.deb /tmp/packages-centreon/centreon-perl*.deb
apt-get clean
rm -rf /var/lib/apt/lists/* \
       /var/tmp/* \
       /usr/share/doc/* \
       /usr/share/man/* \
       /var/cache/apt/* \
       /bin/systemctl

EOF

RUN ln -s /usr/local/bin/systemctl /usr/bin/systemctl && \
    mkdir -p /var/cache/centreon/ && \
    chown www-data:www-data /var/cache/centreon && \
    chmod 775 /var/cache/centreon && \
    chown -R centreon-gorgone:centreon-gorgone /var/lib/centreon-gorgone

# Set the working directory
WORKDIR /var/lib/centreon-gorgone
USER centreon-gorgone

ENTRYPOINT [ "/var/lib/centreon-gorgone/container.sh" ]
# Set a different entrypoint for the new container
CMD ["/usr/bin/perl", "/usr/bin/gorgoned", "--config=/etc/centreon-gorgone/config.yaml", "--severity=debug"]
