#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$COMMIT" ] ; then
  echo "You need to specify COMMIT environment variables."
  exit 1
fi

# Create source tarball.
cd centreon-web
git checkout --detach "$COMMIT"
cd ..

php `dirname $0`/tools/tsmarty2centreon.php centreon-web/www/ > smarty_translate.php
echo "<?php" > centreon-web/www/install/smarty_translate.php
cat smarty_translate.php >> centreon-web/www/install/smarty_translate.php
echo "?>" >> centreon-web/www/install/smarty_translate.php

# Create database with information to translate
cat `dirname $0`/tools/tables.sql | sqlite3 translation.db

# Get menu translation
cat centreon-web/www/install/insertTopology.sql | grev -v LOCK | sqlite3 translation.db
echo "SELECT DISTINCT topology_name FROM topology ORDER BY topology_name ASC;" | sqlite3 translation.db > menu_translation.tmp
echo "<?php" > menu_translation.php
cat menu_translation.tmp | sed -E 's/^(.*)$/_\("\1"\)/g' >> menu_translation.php
echo "?>" >> menu_translation.php

# Get broker form translation
cat centreon-web/www/install/insertBaseConf.sql | sed ':a;N;$!ba;s/\n/ /g' | grep -oE "INSERT INTO \`cb_field\`.+" | cut -d ';' -f 1 > baseConf.sql
echo ";" >> baseConf.sql
cat baseConf.sql | sqlite3 translation.db
echo "SELECT DISTINCT displayname FROM cb_field ORDER BY displayname ASC;" | sqlite3 translation.db > broker_field_translation.tmp
echo "<?php" > broker_field_translation.php
cat broker_field_translation.tmp | sed -E 's/^(.*)$/_\("\1"\)/g' >> broker_field_translation.php
echo "?>" >> broker_field_translation.php
mv broker_field_translation.php centreon-web/broker_field_translation.php

# Generate messages.po file
find centreon-web/ -name '*.php' | egrep -ve "help\.php" > po_src
xgettext --default-domain=messages -k_ --files-from=po_src --output=messages.pot
msgmerge centreon-web/lang/fr/LC_MESSAGES/messages.po messages.pot -o messages.po

# Get help translation
echo "SELECT DISTINCT description FROM cb_field ORDER BY description ASC;" | sqlite3 translation.db > broker_help_translation.tmp
echo "<?php" > broker_help_translation.php
cat broker_help_translation.tmp | sed -E 's/^(.*)$/_\("\1"\)/g' >> broker_help_translation.php
echo "?>" >> broker_help_translation.php
mv broker_help_translation.php centreon-web/help.php

# Generate help.po file
find centreon-web/ -name '*.php' | egrep -e "help\.php" > po_src_help
xgettext --default-domain=messages -k_ --files-from=po_src_help --output=help.pot
msgmerge centreon-web/lang/fr/LC_MESSAGES/help.po help.pot -o help.po

# Clean workspace
rm -f baseConf.sql broker_field_translation.tmp	broker_help_translation.tmp	help.pot menu_translation.php menu_translation.tmp messages.pot po_src po_src_help smarty_translate.php translation.db