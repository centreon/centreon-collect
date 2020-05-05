#############
Configuration
#############

Centreon SSH Connector itself does not require any configuration. It
should only be configured as a connector of Centreon Engine.

To execute SSH check over SSH with Centreon SSH Connector from Centreon
Engine, one might configure commands that relates to SSH check
(like check_by_ssh).

Binary arguments
~~~~~~~~~~~~~~~~

These arguments are centreon_connector_ssh options.

========== ========= ===================================================
Short name Long name Description
========== ========= ===================================================
-d         --debug   If this flag is specified, print all logs messages.
-h         --help    Print help and exit.
-v         --version Print software version and exit.
========== ========= ===================================================

Check arguments
~~~~~~~~~~~~~~~

These arguments are checks options (like check_by_ssh options).

========== ================ =================================================
Short name Long name        Description
========== ================ =================================================
-1         --proto1         This option is not supported.
-2         --proto2         Tell ssh to use Protocol 2.
-4         --use-ipv4       Enable IPv4 connection.
-6         --use-ipv6       Enable IPv6 connection.
-a         --authentication Authentication password.
-C         --command        Command to execute on the remote machine.
-E         --skip-stderr    Ignore all or first n lines on STDERR.
-f         --fork           This option is not supported.
-h         --help           Not used.
-H         --hostname       Host name, IP Address.
-i         --identity       Identity of an authorized key.
-l         --logname        SSH user name on remote host.
-n         --name           This option is not supported.
-o         --ssh-option     This option is not supported.
-O         --output         This option is not supported.
-p         --port           Port number (default 22).
-q         --quiet          Not used.
-s         --services       This option is not supported.
-S         --skip-stdout    Ignore all or first n lines on STDOUT.
-t         --timeout        Seconds before connection times out (default 10).
-v         --verbose        Not used.
-V         --version        Not used.
========== ================ =================================================

Exemple::

  define connector{
    connector_name centreon_connector_ssh
    connector_line /usr/bin/centreon-connector/centreon_connector_ssh
  }

  define command{
    command_name ssh_check_cpu
    command_line $USER1$/check_by_ssh -H $HOSTADDRESS$ -l $_HOSTUSER$ -a $_HOSTPASSWORD$ -C "$USER1$/check_cpu -w $ARG1$ -c $ARG2$"
    connector centreon_connector_ssh
  }

  define command{
    command_name ssh_check_disk
    command_line $USER1$/check_by_ssh -H $HOSTADDRESS$ -l $_HOSTUSER$ -a $_HOSTPASSWORD$ -C "$USER1$/check_disk -D $ARG1$ -w $ARG2$ -c $ARG3$"
    connector centreon_connector_ssh
  }
