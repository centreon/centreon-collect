#!/bin/sh

# Check arguments.
if [ "$1" '!=' '-v' -o -z "$2" ] ; then
  echo "USAGE: $0 -v <VERSION>"
  exit 1
fi

# Browse all modules of project.
cd www/modules
for module in * ; do
  # Get versions.
  BASEVERSION=`grep mod_release $module/conf.php | cut -d '"' -f 4`
  NEXTVERSION="$2"

  # Replace base version with next version in main conf.php.
  sed -i "s/$BASEVERSION/$NEXTVERSION/g" "$module/conf.php"

  # Create update script.
  mkdir -p "$module/UPGRADE/$module-$NEXTVERSION"
  cp "$module/UPGRADE/$module-$BASEVERSION/conf.php" "$module/UPGRADE/$module-$NEXTVERSION/"
  cfgfile="$module/UPGRADE/$module-$NEXTVERSION/conf.php"
  sed -i -e 's/release_from.*/release_from"] = "'$BASEVERSION'";/g' $cfgfile
  sed -i -e 's/release_to.*/release_to"] = "'$NEXTVERSION'";/g' $cfgfile
  sed -i -e 's/sql_files.*/sql_files"] = "0";/g' $cfgfile
  sed -i -e 's/php_files.*/php_files"] = "0";/g' $cfgfile
done
