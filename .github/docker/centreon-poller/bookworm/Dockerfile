FROM debian:bookworm

ARG VERSION
ARG IS_CLOUD
ARG STABILITY

COPY --chmod=755 ./.github/docker/centreon-poller/scripts/systemctl /bin/systemctl

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

apt-get install -y wget gnupg2 ca-certificates

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

apt-get update
apt-get install -y /tmp/packages-centreon/centreon-*.deb
apt-get clean

systemctl stop cbd
systemctl stop centengine

EOF

COPY --chmod=755 ./.github/docker/centreon-poller/bookworm/entrypoint /usr/share/centreon
COPY --chmod=755 ./.github/docker/centreon-poller/init/* /etc/init.d/
COPY --chmod=600 ./.github/docker/centreon-poller/sudoersCentreon /etc/sudoers.d/centreon

# grpc central-module
EXPOSE 51003

# bbdo central-broker
EXPOSE 5669
# grpc central-broker
EXPOSE 51001

# bbdo central-rrd
EXPOSE 5670
# grpc central-rrd
EXPOSE 51002

ENTRYPOINT ["/usr/share/centreon/container.sh"]
