#!/usr/bin/env bash

# Components of this repository that publish a source tarball to the download site, mapped to the
# bucket directory they publish it to. A component absent from this map publishes no tarball and is
# never waited on by the download-site publication.
#
# That absence is load-bearing for centreon-monitoring-agent: it releases from this repository under
# the same bundle tag, but ships GitHub release assets through its own pipeline (MON-208876), so the
# sources publication must not expect a tarball from it.
#
# Keys match the component tag prefix, so tag centreon-collect-25.10.8 -> key centreon-collect.

# shellcheck disable=SC2034  # read by the scripts that source this file
declare -A SOURCE_BUCKET_DIRECTORY

SOURCE_BUCKET_DIRECTORY[centreon-collect]="centreon-collect"
SOURCE_BUCKET_DIRECTORY[centreon-gorgone]="centreon-gorgone"
