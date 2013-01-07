#################
Technical details
#################

This article describes how Centreon Perl Connector allow much gain on
Perl script execution.

First of all let's examine how a Perl script is executed traditionnally
by Centreon Engine.

  * Centreon Engine forks, creating a new separate process.
  * This new process executes the execve syscall to run the Perl
    interpreter. This step does not create a new process.
  * The Perl interpreter parse the Perl script.
  * Perl script get executed.

With Centreon Engine, the same script get executed multiple times but
with different arguments. Therefore we took advantage of this fact to
efficiently parse all the scripts once and get them executed. This was
only possible because of the fork()ing system of Unix-like platform. If
you read the reference page on Wikipedia you indeed remarked that once
fork()ed the old and the new process are identical. Centreon Connector
Perl's steps to execute scripts are as follow.

  * Centreon Engine creates a resident process of Centreon Connector
    Perl once
  * For all Perl scripts execution requests are forwarded to this
    process when requested to execute a script, Centreon Perl Connector
    checks if this script has already been parsed if not it parses it
    using the Embedded Perl interpreter.
  * Centreon Perl Connector forks itself.
  * The precompiled script gets executed

This way Perl scripts are only parsed once during the lifetime of the
monitoring engine. This heavily relates to
`prepared statements <http://en.wikipedia.org/wiki/Prepared_statements>`_
in SQL.
