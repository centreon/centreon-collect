#############
Configuration
#############

FIXME

Centreon Connector SSH itself does not require any configuration. It
should only be configured as a connector of Centreon Engine.

To execute SSH check over SSH with Centreon Connector SSH from Centreon
Engine, one might configure commands that relates to SSH check
(like check_by_ssh).

Exemple::

  define connector{
    connector_name centreon_connector_ssh
    connector_line /usr/bin/centreon-connector/centreon_connector_ssh
  }

  define command{
    command_name check_cpu
    command_line $HOSTADDRESS$ $_HOSTUSER$ $_HOSTPASSWORD$ $USER1$/check_cpu -w $ARG1$ -c $ARG2$
    connector centreon_connector_ssh
  }

  define command{
    command_name check_disk
    command_line $HOSTADDRESS$ $_HOSTUSER$ $_HOSTPASSWORD$ $USER1$/check_disk -D $ARG1$ -w $ARG2$ -c $ARG3$
    connector centreon_connector_ssh
  }
