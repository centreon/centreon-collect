#!/bin/sh

# Check arguments.
if [ -z "$1" -o -z "$2" ] ; then
  echo "USAGE:     $0 <OLDSERIE> <NEWSERIE>"
  echo "  example: $0 18.10 19.04"
  exit 1
fi

# Copy and tweak serie directories.
for olddir in `find . -type d -name $1` ; do
  newdir=`dirname $olddir`/$2
  cp -a "$olddir" "$newdir"
  find "$newdir" -type f | xargs sed -i -e "s#$1#$2#g"
  git add "$newdir"
done

# Add tasks to Jenkins role.
echo "  - import_tasks: $2/jobs-build-dependencies.yml
  - import_tasks: $2/jobs-dependencies.yml
  - import_tasks: $2/jobs-unittest.yml
  - import_tasks: $2/jobs-stable.yml
  - import_tasks: $2/jobs-misc.yml" >> infrastructure/ci/roles/jenkins-master/tasks/main.yml
git add infrastructure/ci/roles/jenkins-master/tasks/main.yml
