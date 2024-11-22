#!/bin/sh

startCentagent() {
  systemctl daemon-reload ||:
  systemctl unmask centagent.service ||:
  systemctl preset centagent.service ||:
  systemctl enable centagent.service ||:
  systemctl restart centagent.service ||:
}


debianLinkNagios() {
  if [ ! -r /usr/lib64/nagios/plugins ]; then
    if [ ! -d /usr/lib64/nagios ]; then
      mkdir -p /usr/lib64/nagios
      chmod 0755 /usr/lib64/nagios
    fi
    ln -s /usr/lib/nagios/plugins /usr/lib64/nagios/plugins
  fi
}

#In debian nagios plugins are stored in /usr/lib/nagios/plugins instead of /usr/lib64/nagios/plugins
#so we create a link /usr/lib/nagios/plugins instead => /usr/lib64/nagios/plugins in order to have
#the same commands configuration for all pollers
if  [ "$1" = "configure" ]; then
  debianLinkNagios
fi


startCentagent

