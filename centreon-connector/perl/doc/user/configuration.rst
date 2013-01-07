#############
Configuration
#############

Centreon Perl Connector itself does not require any configuration. It
should only be configured as a connector of Centreon Engine.

To execute Perl scripts with Centreon Perl Connector from Centreon
Engine, one might configure commands that relates to Perl scripts. Such
commands must only contain the path to the Perl script to execute
followed by its arguments, just like one would on the command line. To
make it simple, you just have to add a connector property to your
command definition.

Binary arguments
~~~~~~~~~~~~~~~~

These arguments are centreon_connector_perl options.

========== ========= ===================================================
Short name Long name Description
========== ========= ===================================================
-d         --debug   If this flag is specified, print all logs messages.
-h         --help    Print help and exit.
-v         --version Print software version and exit.
========== ========= ===================================================

Exemple::

  define connector{
    connector_name centreon_connector_perl
    connector_line /usr/bin/centreon-connector/centreon_connector_perl
  }

  define command{
    command_name check_ping
    command_line $USER1$/check_ping.pl -H $HOSTADDRESS$
    connector centreon_connector_perl
  }

  define command{
    command_name check_disk
    command_line $USER1$/check_disk.pl -H $HOSTADDRESS$ -D $ARG1$
    connector centreon_connector_perl
  }
