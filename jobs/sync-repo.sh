#!/bin/sh

set -e
set -x

SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
IP_REPO='yum.int.centreon.com'
REMOTE_BASE_PATH="/srv/centreon-yum/yum.centreon.com"

PROJECT=""
SUB_PATH=""

# This function will parse the flag passed to the command and assign them to variables
# If mandatories options are missing an error is returned
parse_command_options () {
  while [ $# -gt 0 ] ; do
    case $1 in
        --project)
          set_variable "PROJECT" "$2"
          shift 2
          ;;
        --path)
          set_variable "SUB_PATH" "$2"
          shift 2
          ;;
        --confirm)
          set_variable "CONFIRM" true
          shift 1
          ;;
        --help)
          usage
          exit 0
          ;;
        *)
          echo "ERROR - Unrecognized parameter ${1}"
          usage
          exit 1
          ;;
    esac
  done

  # Return an error if mandatory parameters are missing
  if [ "$PROJECT" = "" ] ; then
    echo "ERROR - Missing Parameters"
    usage
    exit 1
  fi
}

set_variable () {
  local varname=$1
  shift
  eval "$varname=\"$*\""
}

# Display usage message
usage () {
  echo
  echo "This script will synchronize given repository"
  echo "Global Options:"
  echo "  --project   <mandatory>     project to synchronize (standard|bam|mbi|map|epp)"
  echo "  --path      <optional>      path to synchronize (eg: /21.04/el7/testing/noarch)"
  echo "  --confirm   <optional>      confirm synchronization (dry run if not given)"
}


# Get all the flag and assign them to variable
parse_command_options "$@"


if [ "$PROJECT" = "standard" ] ; then
  LOCAL_PATH=/srv/yum/standard
  REMOTE_PROJECT_PATH=standard
elif [ "$PROJECT" = "business" ] ; then
  LOCAL_PATH=/srv/yum/business
  REMOTE_PROJECT_PATH=centreon-business/1a97ff9985262bf3daf7a0919f9c59a6
elif [ "$PROJECT" = "lts" ] ; then
  LOCAL_PATH=/srv/yum/lts
  REMOTE_PROJECT_PATH=centreon-lts/1c0143e9c46ef424bc0c1ae9ba77203c17abf1cda7e
elif [ "$PROJECT" = "bam" ] ; then
  LOCAL_PATH=/srv/yum/bam
  REMOTE_PROJECT_PATH=centreon-bam/d4e1d7d3e888f596674453d1f20ff6d3
elif [ "$PROJECT" = "map" ] ; then
  LOCAL_PATH=/srv/yum/map
  REMOTE_PROJECT_PATH=centreon-map/bfcfef6922ae08bd2b641324188d8a5f
elif [ "$PROJECT" = "mbi" ] ; then
  LOCAL_PATH=/srv/yum/mbi
  REMOTE_PROJECT_PATH=centreon-mbi/5e0524c1c4773a938c44139ea9d8b4d7
elif [ "$PROJECT" = "plugin-packs" ] ; then
  LOCAL_PATH=/srv/yum/plugin-packs
  REMOTE_PROJECT_PATH=plugin-packs/2e83f5ff110c44a9cab8f8c7ebbe3c4f
elif [ "$PROJECT" = "failover" ] ; then
  LOCAL_PATH=/srv/yum/failover
  REMOTE_PROJECT_PATH=centreon-failover/c2f6eb8f4e8f289c80b90c9b01625c55
fi

if [ "$SUB_PATH" = "/" ] ; then
  SUB_PATH = ""
fi

REMOTE_PATH=$REMOTE_BASE_PATH/$REMOTE_PROJECT_PATH

$SSH_REPO ssh -o StrictHostKeyChecking=no "cesync@yum.int.centreon.com" mkdir -p $REMOTE_PATH$SUB_PATH

if [ "$CONFIRM" = true ] ; then
  echo 'CONFIRM MODE : Modifications will be made on online repositories'
  $SSH_REPO rsync -chlro --stats --progress --delete-before $LOCAL_PATH$SUB_PATH/ cesync@$IP_REPO:$REMOTE_PATH$SUB_PATH/
  $SSH_REPO aws cloudfront create-invalidation --distribution-id E34EBWWERP6QET --paths /$REMOTE_PROJECT_PATH$SUB_PATH/'*'
else
  echo 'DRY RUN MODE : No modification will be made on online repositories'
  $SSH_REPO --dry-run -chlro --stats --progress --delete-before $LOCAL_PATH$SUB_PATH/ cesync@$IP_REPO:$REMOTE_PATH$SUB_PATH/
fi

exit
