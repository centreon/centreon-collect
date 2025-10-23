FROM debian:12-slim

ARG VERSION
ARG IS_CLOUD
ARG STABILITY

ENV DEBIAN_FRONTEND=noninteractive

RUN groupadd -g 900 centreon &&\
    useradd -u 900 -g centreon -m -r -d /var/spool/centreon -c "Centreon" -s /bin/bash centreon &&\
    groupadd -g 901 centreon-engine &&\
    useradd -u 901 -g centreon-engine -m -r -d /var/lib/centreon-engine -c "Centreon Engine" -s /bin/bash centreon-engine &&\
    groupadd -g 902 centreon-broker &&\
    useradd -u 902 -g centreon-broker -m -r -d /var/lib/centreon-broker -c "Centreon Broker"  -s /bin/bash centreon-broker &&\
    groupadd -g 903 centreon-gorgone &&\
    useradd -u 903 -g centreon-gorgone -m -r -d /var/lib/centreon-gorgone -c "Centreon Gorgone"  -s /bin/bash centreon-gorgone &&\
    usermod -aG centreon-broker centreon-engine  && \
    usermod -aG centreon-engine centreon-broker && \
    usermod -aG centreon-gorgone centreon-engine && \
    usermod -aG centreon centreon-engine && \
    usermod -aG sudo centreon-engine

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

######################################
# install and configure repositories #
######################################
apt-get update
apt-get install -y --no-install-recommends \
                    wget \
                    gnupg2 \
                    ca-certificates \
                    python3-minimal \
                    python3.11-venv \
                    software-properties-common \
                    libmariadb3 \
                    monitoring-plugins-basic \
                    sudo
                    

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

printf '%s\n' '#!/bin/sh' 'exit 101' > /usr/sbin/policy-rc.d
chmod +x /usr/sbin/policy-rc.d
printf '%s\n' '#!/bin/sh' 'exit 0' > /bin/systemctl
chmod +x /bin/systemctl

mkdir -p /tmp/debs-to-install
find /tmp/packages-centreon -name "centreon*.deb" ! -name "*broker-cbd*" -exec cp {} /tmp/debs-to-install/ \;
apt-get install -y /tmp/debs-to-install/centreon*.deb
rm -rf /tmp/debs-to-install
apt-get clean
rm -rf /var/lib/apt/lists/* \
       /tmp/* \
       /var/tmp/* \
       /usr/share/doc/* \
       /usr/share/man/* \
       /var/cache/apt/* \
       /usr/sbin/policy-rc.d \
       /bin/systemctl

EOF

RUN echo '{"app_secret":"","salt":""}' > /etc/centreon-engine/engine-context.json &&\
    mkdir -p /var/lib/centreon/centplugins/ &&\
    chown centreon-engine:centreon-engine /var/lib/centreon/centplugins/

COPY --chmod=755 --chown=centreon-engine:centreon-engine ./.github/docker/centreon-engine/bookworm/entrypoint /var/lib/centreon-engine
COPY --chmod=600 ./.github/docker/centreon-engine/sudoersCentreon /etc/sudoers.d/centreon

# grpc central-module
EXPOSE 51003

# bbdo central-broker
# EXPOSE 5669
# grpc central-broker
EXPOSE 51001

# bbdo central-rrd
# EXPOSE 5670
# grpc central-rrd
EXPOSE 51002

WORKDIR /var/lib/centreon-engine

USER centreon-engine

ENTRYPOINT ["/var/lib/centreon-engine/container.sh"]

CMD ["uvicorn", "api_control:app", "--host", "0.0.0.0", "--port", "8000"]
