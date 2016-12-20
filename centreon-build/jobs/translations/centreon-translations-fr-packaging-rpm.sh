#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$CENTREONCOMMIT" -o -z "$TRANSLATIONCOMMIT" -o -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT, VERSION and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
docker pull ci.int.centreon.com:5000/mon-build-dependencies:centos6
docker pull ci.int.centreon.com:5000/mon-build-dependencies:centos7

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos6
mkdir output-centos6
rm -rf output-centos7
mkdir output-centos7

# Get Centreon Web sources.
rm -rf centreon-web
git clone https://github.com/centreon/centreon centreon-web
cd centreon-web
git checkout --detach "$CENTREONCOMMIT"
cd ..

rm -rf centreon-translations
git clone https://kduret@github.com/centreon/centreon-translations centreon-translations

# Create source tarball.
cd centreon-translations
git checkout --detach "$TRANSLATIONCOMMIT"
rm -rf "../centreon-translations-fr-$VERSION"
mkdir "../centreon-translations-fr-$VERSION"
git archive HEAD | tar -C "../centreon-translations-fr-$VERSION" -x
cd ..
tar czf "input/centreon-translations-fr-$VERSION.tar.gz" "centreon-translations-fr-$VERSION"

php `dirname $0`/tools/tsmarty2centreon.php centreon-web/www/ > smarty_translate.php
echo "<?php" > centreon-web/www/install/smarty_translate.php
cat smarty_translate.php >> centreon-web/www/install/smarty_translate.php
echo "?>" >> centreon-web/www/install/smarty_translate.php

# Create topologies database
cat `dirname $0`/tools/tables.sql | sqlite3 translation.db

cat centreon-web/www/install/insertTopology.sql | grev -v LOCK | sqlite3 translation.db
echo "SELECT DISTINCT topology_name FROM topology ORDER BY topology_name ASC;" | sqlite3 translation.db > menu_translation.tmp
echo "<?php" > menu_translation.php
cat menu_translation.tmp | sed -E 's/^(.*)$/_\("\1"\)/g' >> menu_translation.php
echo "?>" >> menu_translation.php

cat centreon-web/www/install/insertBaseConf.sql | sed ':a;N;$!ba;s/\n/ /g' | grep -oE "INSERT INTO \`cb_field\`.+" | cut -d ';' -f 1 > baseConf.sql
echo ";" >> baseConf.sql
cat baseConf.sql | sqlite3 translation.db
echo "SELECT DISTINCT displayname FROM cb_field ORDER BY displayname ASC;" | sqlite3 translation.db > broker_field_translation.tmp
echo "<?php" > broker_field_translation.php
cat broker_field_translation.tmp | sed -E 's/^(.*)$/_\("\1"\)/g' >> broker_field_translation.php
echo "?>" >> broker_field_translation.php
mv broker_field_translation.php centreon-web/broker_field_translation.php

find centreon-web/ -name '*.php' | egrep -ve "help\.php" > po_src
xgettext --default-domain=messages -k_ --files-from=po_src --output=messages.pot
msgmerge centreon-translations/fr/LC_MESSAGES/messages.po messages.pot -o messages_$VERSION.po


echo "SELECT DISTINCT description FROM cb_field ORDER BY description ASC;" | sqlite3 translation.db > broker_help_translation.tmp
echo "<?php" > broker_help_translation.php
cat broker_help_translation.tmp | sed -E 's/^(.*)$/_\("\1"\)/g' >> broker_help_translation.php
echo "?>" >> broker_help_translation.php
mv broker_help_translation.php centreon-web/help.php

find centreon-web/ -name '*.php' | egrep -e "help\.php" > po_src_help
xgettext --default-domain=messages -k_ --files-from=po_src_help --output=help.pot
msgmerge centreon-translations/fr/LC_MESSAGES/help.po help.pot -o help_$VERSION.po



# Build RPMs.
#cp centreon-export/packaging/centreon-export.spectemplate input/
#docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output-centos6
#docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input output-centos7

# Copy files to server.
#FILES_CENTOS6='output-centos6/noarch/*.rpm'
#FILES_CENTOS7='output-centos7/noarch/*.rpm'
#scp -o StrictHostKeyChecking=no $FILES_CENTOS6 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/internal/el6/noarch/RPMS"
#scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/internal/el7/noarch/RPMS"
#ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/internal/el6/noarch
#ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/internal/el7/noarch
