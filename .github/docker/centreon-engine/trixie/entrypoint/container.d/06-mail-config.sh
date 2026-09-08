#!/bin/sh

# Wire the email notification path (both options: native msmtp/mail command
# and the centreon-plugin-notification-email connector) from SMTP_* env vars.
# If SMTP_HOST is empty, notifications simply won't have anywhere to send
# mail to (consistent with the previous no-op state) but boot never fails.

if [ -n "$SMTP_HOST" ]; then
    TLS_LINE="tls off"
    if [ "$SMTP_TLS" = "on" ] || [ "$SMTP_TLS" = "1" ]; then
        TLS_LINE="tls on"
    fi

    # container.sh runs as the unprivileged centreon-engine user (USER
    # centreon-engine in the Dockerfile), so /etc/msmtprc is not writable.
    # MSMTPRC (set in the Dockerfile ENV) points msmtp at a writable path
    # instead - and since it's an exported env var rather than $HOME, it's
    # inherited the same way by centengine itself and by every notification
    # command it forks, regardless of how each child's environment is built.
    cat > "$MSMTPRC" <<EOF
defaults
$TLS_LINE
tls_starttls on

account relay
host $SMTP_HOST
port $SMTP_PORT
from $SMTP_FROM

account default : relay
EOF
    chmod 600 "$MSMTPRC"
fi

RESOURCE_CFG="/etc/centreon-engine/resource.cfg"

if [ -f "$RESOURCE_CFG" ]; then
    sed -i "s|^\$SMTPADDRESS\$=.*|\$SMTPADDRESS\$=$SMTP_HOST|" "$RESOURCE_CFG"
    sed -i "s|^\$SMTPPORT\$=.*|\$SMTPPORT\$=$SMTP_PORT|" "$RESOURCE_CFG"
    sed -i "s|^\$SMTPFROMADDRESS\$=.*|\$SMTPFROMADDRESS\$=$SMTP_FROM|" "$RESOURCE_CFG"
fi
