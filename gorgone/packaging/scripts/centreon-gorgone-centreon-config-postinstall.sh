#!/bin/bash

fixConfigurationFileRights() {
  # force update of configuration file rights since they are not updated automatically by nfpm.
  # Also runs on fresh install: if another package (e.g. centreon-auto-discovery-server) drops a
  # file into /etc/centreon-gorgone/config.d before this package installs, dpkg creates that
  # directory with default root:root ownership and never corrects it afterwards.
  chown centreon-gorgone:centreon-gorgone /etc/centreon-gorgone/config.d
  chmod 0770 /etc/centreon-gorgone/config.d
  chown centreon-gorgone:centreon-gorgone /etc/centreon-gorgone/config.d/cron.d
  chmod 0770 /etc/centreon-gorgone/config.d/cron.d
  chmod 0640 /etc/centreon-gorgone/config.d/30-centreon.yaml
  # 31-centreon-api.yaml must stay group-writable: the web install wizard
  # (configFileSetup.php) writes the Gorgone API credentials into it via the
  # centreon-gorgone group, gated on is_writable() - 0640 silently skips that
  # write and leaves the @GORGONE_USER@/@GORGONE_PASSWORD@ placeholders in
  # place. Matches the 0660 declared in centreon-gorgone-centreon-config.yaml.
  chmod 0660 /etc/centreon-gorgone/config.d/31-centreon-api.yaml
  chmod 0640 /etc/centreon-gorgone/config.d/50-centreon-audit.yaml
}

addGorgoneSshKeys() {
  if [ ! -d /var/lib/centreon-gorgone/.ssh ] && [ -d /var/spool/centreon/.ssh ]; then
    cp -r /var/spool/centreon/.ssh /var/lib/centreon-gorgone/.ssh
    chown -R centreon-gorgone:centreon-gorgone /var/lib/centreon-gorgone/.ssh
    chmod 600 /var/lib/centreon-gorgone/.ssh/id_rsa
  fi
}

action="$1"
if  [ "$1" = "configure" ] && [ -z "$2" ]; then
  # Alpine linux does not pass args, and deb passes $1=configure
  action="install"
elif [ "$1" = "configure" ] && [ -n "$2" ]; then
  # deb passes $1=configure $2=<current version>
  action="upgrade"
fi

case "$action" in
  "1" | "install")
    fixConfigurationFileRights
    addGorgoneSshKeys
    ;;
  "2" | "upgrade")
    fixConfigurationFileRights
    addGorgoneSshKeys
    ;;
  *)
    # $1 == version being installed
    fixConfigurationFileRights
    addGorgoneSshKeys
    ;;
esac
