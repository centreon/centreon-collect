#!/bin/sh

set -e
set -x

# Install development repositories.
sed -i -e 's/@DISTRIB@/leap/g' -e 's/@VERSION@/3.5/g' -e 's/gpgcheck=1/gpgcheck=0/g' /tmp/centreon-internal.repo.in
zypper --non-interactive ar /tmp/centreon-internal.repo.in

# Install required build dependencies for all Centreon projects.
zypper --non-interactive install rpm-build
xargs zypper --non-interactive install --download-only < /tmp/build-dependencies.txt

# Install Node.js and related elements.
zypper --non-interactive install nodejs8 npm8 system-user-nobody
npm install -g gulp

# Install Composer.
zypper --non-interactive install curl php7 php7-dom php7-json php7-mbstring php7-openssl php7-phar
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer

# Install fake yum-builddep binary to install build dependencies.
cat > /usr/local/bin/yum-builddep <<EOF
#!/bin/sh
zypper --non-interactive install \`rpmspec -q --buildrequires \$3 | cut -d '<' -f 1 | cut -d '>' -f 1 | cut -d ' ' -f 1 | sort -u\`
EOF
chmod +x /usr/local/bin/yum-builddep
