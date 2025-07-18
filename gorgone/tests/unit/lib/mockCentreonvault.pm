package tests::unit::lib::mockCentreonvault;
use strict;
use warnings;

# we can't use mock() on a non loaded package, so we need to create the class we want to mock first.
# We could have set centreon-common as a dependancy for the test, but it's not that package we are testing right now, so let mock it.
BEGIN {
    package centreon::common::centreonvault;
    sub get_secret {};
    sub new {};
    $INC{ (__PACKAGE__ =~ s{::}{/}rg) . ".pm" } = 1;
}

1;
