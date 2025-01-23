FROM almalinux:9

ARG VERSION
ARG IS_CLOUD
ARG STABILITY

COPY --chmod=755 ./.github/docker/centreon-poller/alma9/entrypoint /usr/share/centreon
COPY --chmod=755 ./.github/docker/centreon-poller/scripts/systemctl /bin/systemctl
COPY --chmod=755 ./.github/docker/centreon-poller/init/* /etc/init.d/

RUN --mount=type=bind,src=packages-centreon,dst=/tmp/packages-centreon \
    --mount=type=secret,id=ARTIFACTORY_INTERNAL_REPO_USERNAME \
    --mount=type=secret,id=ARTIFACTORY_INTERNAL_REPO_PASSWORD \
    bash -e <<EOF

echo 'install_weak_deps=False' >> /etc/dnf/dnf.conf

dnf install -y dnf-plugins-core

if [[ "${IS_CLOUD}" == "true" ]]; then
  dnf config-manager --add-repo https://$(cat /run/secrets/ARTIFACTORY_INTERNAL_REPO_USERNAME):$(cat /run/secrets/ARTIFACTORY_INTERNAL_REPO_PASSWORD)@packages.centreon.com/rpm-standard-internal/${VERSION}/el9/centreon-${VERSION}-internal.repo
  sed -i "s#packages.centreon.com/rpm-standard-internal#$(cat /run/secrets/ARTIFACTORY_INTERNAL_REPO_USERNAME):$(cat /run/secrets/ARTIFACTORY_INTERNAL_REPO_PASSWORD)@packages.centreon.com/rpm-standard-internal#" /etc/yum.repos.d/centreon-${VERSION}-internal.repo
else
  dnf config-manager --add-repo https://packages.centreon.com/rpm-standard/${VERSION}/el9/centreon-${VERSION}.repo
fi

if [[ "$STABILITY" == "testing" ]]; then
  dnf config-manager --set-disabled 'centreon*unstable*'
elif [[ "$STABILITY" == "stable" ]]; then
  dnf config-manager --set-disabled 'centreon*unstable*' --set-disabled 'centreon*testing*'
fi
dnf config-manager --set-disabled 'centreon-plugin*unstable*' --set-disabled 'centreon-plugin*testing*'

dnf install -y /tmp/packages-centreon/centreon-*.rpm

systemctl stop gorgoned
systemctl stop centengine
systemctl stop cbd

dnf clean all

EOF

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
