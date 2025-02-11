#!/bin/sh

WEB_ADDR=${WEB_ADDR:-web}
WEB_GORGONE_PORT=${WEB_GORGONE_PORT:-8085}
WEB_API_USERNAME=${WEB_API_USERNAME:-admin}
WEB_API_PASSWORD=${WEB_API_PASSWORD:-Centreon!2021}

if getent hosts "${WEB_ADDR}"; then
  # Set HOSTNAME if not already set
  : "${HOSTNAME:=$(hostname)}"

  API_RESPONSE=$(curl -s -X POST --insecure -H "Content-Type: application/json" \
      -d "{\"security\":{\"credentials\":{\"login\":\"${WEB_API_USERNAME}\", \"password\":\"${WEB_API_PASSWORD}\"}}}" \
      "http://${WEB_ADDR}/centreon/api/latest/login")

  API_TOKEN=$(echo "${API_RESPONSE}" | grep -o '"token":"[^"]*' | cut -d'"' -f4)

  PAYLOAD='{"name":"'"$HOSTNAME"'","hostname":"'"$HOSTNAME"'","type":"'"poller"'","address":"'"$HOSTNAME"'","parent_address":"'"127.0.0.1"'"}'

  curl -s -X POST --insecure -i -H "Content-Type: application/json" -H "X-AUTH-TOKEN: ${API_TOKEN}" \
      -d "${PAYLOAD}" \
      "http://${WEB_ADDR}/centreon/api/latest/platform/topology"

  CENTRAL_IP_ADDRESS=$(getent hosts "${WEB_ADDR}" | awk '{print $1;}')
  curl -s -X POST --insecure -i -H "Content-Type: application/json" -H "centreon-auth-token: ${API_TOKEN}" \
      -d "{\"linked_remote_master\":\"\",\"linked_remote_slaves\":[],\"open_broker_flow\":false,\"centreon_central_ip\":\"${CENTRAL_IP_ADDRESS}\",\"server_ip\":\"$HOSTNAME\",\"server_name\":\"$HOSTNAME\",\"server_type\":\"poller\"}" \
      "http://${WEB_ADDR}/centreon/api/index.php?object=centreon_configuration_remote&action=linkCentreonRemoteServer"

  until [ "$THUMBPRINT" != '' ]; do
    sleep 1
    API_RESPONSE=$(curl -s -X GET -H "accept: application/json" "http://${WEB_ADDR}:${WEB_GORGONE_PORT}/api/internal/thumbprint")
    THUMBPRINT=$(echo $API_RESPONSE | grep -o '"thumbprint":\"[^\"]*' | cut -d'"' -f4)
  done

  cat <<EOF > /etc/centreon-gorgone/config.d/40-gorgoned.yaml
name:  gorgoned-$HOSTNAME
description: Configuration for poller $HOSTNAME
gorgone:
  gorgonecore:
    id: 2
    external_com_type: tcp
    external_com_path: "*:5556"
    authorized_clients:
      - key: $THUMBPRINT
    privkey: "/var/lib/centreon-gorgone/.keys/rsakey.priv.pem"
    pubkey: "/var/lib/centreon-gorgone/.keys/rsakey.pub.pem"
  modules:
    - name: action
      package: gorgone::modules::core::action::hooks
      enable: true
      whitelist_cmds: true
      allowed_cmds:
        - ^sudo\s+(/bin/)?systemctl\s+(reload|restart)\s+(centengine|centreontrapd|cbd)\s*$
        - ^(sudo\s+)?(/usr/bin/)?service\s+(centengine|centreontrapd|cbd|cbd-sql)\s+(reload|restart)\s*$
        - ^/usr/sbin/centenginestats\s+-c\s+/etc/centreon-engine/centengine\.cfg\s*$
        - ^cat\s+/var/lib/centreon-engine/[a-zA-Z0-9\-]+-stats\.json\s*$
        - ^/usr/lib/centreon/plugins/.*$
        - ^/bin/perl /usr/share/centreon/bin/anomaly_detection --seasonality >> /var/log/centreon/anomaly_detection\.log 2>&1\s*$
        - ^/usr/bin/php -q /usr/share/centreon/cron/centreon-helios\.php >> /var/log/centreon-helios\.log 2>&1\s*$
        - ^centreon
        - ^mkdir
        - ^/usr/share/centreon/www/modules/centreon-autodiscovery-server/script/run_save_discovered_host
        - ^/usr/share/centreon/bin/centreon -u \"centreon-gorgone\" -p \S+ -w -o CentreonWorker -a processQueue$

    - name: engine
      package: gorgone::modules::centreon::engine::hooks
      enable: true
      command_file: "/var/lib/centreon-engine/rw/centengine.cmd"

EOF

  systemctl start gorgoned

  until [ "$PING_OK" -gt "0" ] 2>/dev/null; do
    sleep 1
    PING_OK=$(curl -s -X GET -H "accept: application/json" "http://${WEB_ADDR}:${WEB_GORGONE_PORT}/api/internal/constatus" | grep -o '"ping_ok":[0-9]*' | cut -d':' -f2)
    if [ "$PING_OK" -gt "0" ] 2>/dev/null; then
      for action in POLLERGENERATE CFGMOVE POLLERRESTART; do
        curl -X POST --insecure -H "Content-Type: application/json" -H "centreon-auth-token: ${API_TOKEN}" \
            -d "{\"action\":\"$action\",\"values\":\"$HOSTNAME\"}" \
            "http://${WEB_ADDR}/centreon/api/index.php?action=action&object=centreon_clapi"
      done
    fi
  done
fi
