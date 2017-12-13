#!/bin/sh

set -e
set -x

# Install required build dependencies for all Centreon projects.
zypper --non-interactive install rpm-build
xargs zypper --non-interactive install --download-only < /tmp/build-dependencies.txt

# Install fake yum-builddep binary to install build dependencies.
cat > /usr/local/bin/yum-builddep <<EOF
#!/bin/sh
zypper --non-interactive install \`rpmspec -q --buildrequires \$3 | cut -d '<' -f 1 | cut -d '>' -f 1 | cut -d ' ' -f 1 | sort -u\`
EOF
chmod +x /usr/local/bin/yum-builddep
