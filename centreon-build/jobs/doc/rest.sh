#!/bin/sh

# Check arguments.
if [ -z "$PROJECT" -o -z "$TAG" ] ; then
  echo "You need to specify PROJECT and TAG environment variables."
  exit 1
fi
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
SSH_DOC="$SSH_REPO ssh -o StrictHostKeyChecking=no ubuntu@10.24.1.54"

# English doc.
if [ "$ENGLISH" '=' true ] ; then
  $SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos $PROJECT -V $TAG -p'"
fi

# French doc.
if [ -n "$FRENCH" '=' true ] ; then
  $SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos $PROJECT -V $TAG -p'"
fi
