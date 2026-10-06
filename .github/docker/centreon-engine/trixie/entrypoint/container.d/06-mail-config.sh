#!/bin/sh

# Wire the native msmtp/mail notification command from SMTP_* env vars.
# If SMTP_HOST is empty, notifications simply won't have anywhere to send
# mail to (consistent with the previous no-op state) but boot never fails.

if [ -n "$SMTP_HOST" ]; then
    TLS_LINE="tls off"
    if [ "$SMTP_TLS" = "on" ] || [ "$SMTP_TLS" = "1" ]; then
        TLS_LINE="tls on"
    fi

    # centengine spawns notification commands with a from-scratch environment
    # (only NAGIOS_*/macro vars, see common/process/src/spawnp_launcher.cc's
    # posix_spawnp call) - no $HOME, no $MSMTPRC, nothing inherited from this
    # script's own environment. msmtp's only env-independent lookup is the
    # fixed /etc/msmtprc path, pre-created in the Dockerfile as writable by
    # centreon-engine (the file's own owner/mode allow overwriting its
    # contents even though /etc itself stays root-owned).
    cat > /etc/msmtprc <<EOF
defaults
$TLS_LINE
tls_starttls on

account relay
host $SMTP_HOST
port $SMTP_PORT
from $SMTP_FROM

account default : relay
EOF
    chmod 600 /etc/msmtprc
fi
