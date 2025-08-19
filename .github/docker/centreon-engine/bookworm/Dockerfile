FROM debian:12-slim

ARG STABILITY
ARG VERSION
ARG IS_CLOUD


ENV DEBIAN_FRONTEND=noninteractive

RUN --mount=type=secret,id=ARTIFACTORY_INTERNAL_REPO_USERNAME \
    --mount=type=secret,id=ARTIFACTORY_INTERNAL_REPO_PASSWORD \
    --mount=type=bind,src=packages-centreon,dst=/tmp/packages-centreon bash -e <<EOF

    echo 'Acquire::Retries "10"; \
Acquire::https::Timeout "300"; \
Acquire::http::Timeout "300"; \
APT::Install-Recommends "false"; \
APT::Install-Suggests "false";' > /etc/apt/apt.conf.d/99custom

VERSION_CODENAME=\$(
  . /etc/os-release
  echo \$VERSION_CODENAME
)

if [[ "${IS_CLOUD}" == "true" ]]; then
  echo "deb https://$(cat /run/secrets/ARTIFACTORY_INTERNAL_REPO_USERNAME):$(cat /run/secrets/ARTIFACTORY_INTERNAL_REPO_PASSWORD)@packages.centreon.com/apt-standard-internal-unstable/ \$VERSION_CODENAME main" | tee -a /etc/apt/sources.list.d/centreon-unstable.list
else
  echo "deb https://packages.centreon.com/apt-standard-${VERSION}-stable/ \$VERSION_CODENAME main" | tee -a /etc/apt/sources.list.d/centreon-stable.list
  echo "deb https://packages.centreon.com/apt-standard-${VERSION}-testing/ \$VERSION_CODENAME main" | tee -a /etc/apt/sources.list.d/centreon-testing.list
  echo "deb https://packages.centreon.com/apt-standard-${VERSION}-unstable/ \$VERSION_CODENAME main" | tee -a /etc/apt/sources.list.d/centreon-unstable.list
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

groupadd -g 6000 centreon
useradd -u 6000 -g centreon -m -r -d /var/spool/centreon -c "Centreon" -s /bin/bash centreon
groupadd -g 6001 centreon-engine
useradd -u 6001 -g centreon-engine -m -r -d /var/lib/centreon-engine -c "Centreon Engine" -s /bin/bash centreon-engine
groupadd -g 6002 centreon-broker
useradd -u 6002 -g centreon-broker -m -r -d /var/lib/centreon-broker -c "Centreon Broker"  -s /bin/bash centreon-broker
groupadd -g 6003 centreon-gorgone
useradd -u 6003 -g centreon-gorgone -m -r -d /var/lib/centreon-gorgone -c "Centreon Gorgone"  -s /bin/bash centreon-gorgone

apt-get update
apt-get install -y \
    lsb-release \
    ca-certificates \
    apt-transport-https \
    software-properties-common \
    wget \
    gnupg2 \
    curl \
    libmariadb3 \
    monitoring-plugins-basic \
    python3 \
    python3-venv \
    iputils-ping

apt-get install -y /tmp/packages-centreon/centreon-engine-daemon*.deb
apt-get clean

mkdir -p /etc/centreon-broker/ /var/log/centreon-engine/ /var/log/centreon-broker/ /var/lib/centreon-engine/rw
touch /var/log/centreon-engine/retention.dat
chown -R centreon-engine:centreon-engine /etc/centreon-engine
chmod 775 /etc/centreon-engine /etc/centreon-broker
chmod +x /usr/share/centreon/lib/centreon-broker/*

EOF

COPY --chmod=755 ./.github/docker/centreon-engine/bookworn/entrypoint /usr/share/centreon
#COPY --chmod=600 ./.github/docker/centreon-poller/sudoersCentreon /etc/sudoers.d/centreon
COPY --chmod=755 ./.github/docker/centreon-engine/scripts/api_control.py /var/lib/centreon-engine/api_control.py

ENTRYPOINT ["/usr/share/centreon/entrypoint.sh"]
