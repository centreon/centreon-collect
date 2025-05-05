
package tests::unit::lib::mockLogger;
use strict;
use warnings;

# we can't use mock() on a non loaded package, so we need to create the class we want to mock first.
# We could have set centreon-common as a dependancy for the test, but it's not that package we are testing right now, so let mock it.
BEGIN {
    package centreon::common::logger;
    sub severity {};
    sub new {return bless({}, 'centreon::common::logger');}
    sub writeLogFatal {};
    sub writeLogError {};
    sub writeLogWarning {};
    sub writeLogNotice {};
    sub writeLogInfo {};
    sub writeLogDebug {};

    $INC{ (__PACKAGE__ =~ s{::}{/}rg) . ".pm" } = 1; # this allow the module to be available for other modules anywhere in the code.
}


1;