#!/usr/bin/perl

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

# same here, gorgone use a logger, but we don't want to test it right now, so we mock it.
BEGIN {
    package centreon::common::logger;
    sub writeLogInfo { };
    sub writeLogError { };
    $INC{ (__PACKAGE__ =~ s{::}{/}rg) . ".pm" } = 1; # this allow the module to be available for other modules anywhere in the code.
}

package main;

use FindBin;
use lib "$FindBin::Bin/../../../";

use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};

use gorgone::modules::centreon::anomalydetection::hooks;
use gorgone::modules::centreon::anomalydetection::class;

sub create_data_set {
    my $set = {};
    # as we are in unit test, we can't be sure of our current path, but the tests require that we start from the same directory than the script.
    chdir($FindBin::Bin);
    $set->{logger} = mock('centreon::common::logger'); # this is from Test2::Tools::Mock, included by Test2::V0
    $set->{class_object_centreon} = mock ('gorgone::class::sqlquery');

    return $set;
}

sub test_proxy_url($$) {
    my ($set,$data) = @_;

    my $gorgone = gorgone::modules::centreon::anomalydetection::class->new(
        logger => $set->{logger}->class,
        module_id => gorgone::modules::centreon::anomalydetection::hooks::NAME,
        config_core => { gorgonecore => { internal_com_crypt => 0 } }
    );

    # Override custom_execute to return the data we want to test
    $set->{class_object_centreon}->override('custom_execute' => sub { return (1, $data) } );

    $gorgone->{class_object_centreon} = $set->{class_object_centreon}->class;

    $gorgone->connection_informations();
    $set->{class_object_centreon}->reset('custom_execute');

    return $gorgone->{proxy_url};
}

# Test cases and expected results
my @test_cases = (
    { label => 'URL with scheme, username/password and port',
      expected => 'https://user:pa$$w0rd@www.test.com:123',
      data => [ ['saas_url', 'fake'], ['saas_token', 'fake'], ['proxy_url', 'https://www.test.com'], ['proxy_port', '123'], ['proxy_user', 'user'], ['proxy_password', 'pa$$w0rd'], ],
    },
    { label => 'URL without scheme, with username/password and port',
      expected => 'http://user:pa$$w0rd@www.test.com:123',
      data => [ ['saas_url', 'fake'], ['saas_token', 'fake'], ['proxy_url', 'www.test.com'], ['proxy_port', '123'], ['proxy_user', 'user'], ['proxy_password', 'pa$$w0rd'], ],
    },
    { label => 'URL without username/password, with scheme and port',
      expected => 'http://www.test.com:123',
      data => [ ['saas_url', 'fake'], ['saas_token', 'fake'], ['proxy_url', 'http://www.test.com'], ['proxy_port', '123'], ['proxy_user', ''], ['proxy_password', ''], ],
    },
    { label => 'URL with scheme, without username/password and port',
      expected => 'http://www.test.com',
      data => [ ['saas_url', 'fake'], ['saas_token', 'fake'], ['proxy_url', 'http://www.test.com'], ['proxy_port', ''], ['proxy_user', ''], ['proxy_password', ''], ],
    },
    { label => 'URL with scheme and username/password, without port',
      expected => 'http://user:pa$$w0rd@www.test.com',
      data => [ ['saas_url', 'fake'], ['saas_token', 'fake'], ['proxy_url', 'http://www.test.com'], ['proxy_port', ''], ['proxy_user', 'user'], ['proxy_password', 'pa$$w0rd'], ],
    },
    { label => 'URL without scheme',
      expected => 'http://www.test.com',
      data => [ ['saas_url', 'fake'], ['saas_token', 'fake'], ['proxy_url', 'www.test.com'], ['proxy_port', ''], ['proxy_user', ''], ['proxy_password', ''], ],
    },
    { label => 'URL with only username/password',
      expected => 'http://user:pa$$w0rd@www.test.com',
      data => [ ['saas_url', 'fake'], ['saas_token', 'fake'], ['proxy_url', 'www.test.com'], ['proxy_port', ''], ['proxy_user', 'user'], ['proxy_password', 'pa$$w0rd'], ],
    },
    { label => 'URL with only port',
      expected => 'http://www.test.com:123',
      data => [ ['saas_url', 'fake'], ['saas_token', 'fake'], ['proxy_url', 'www.test.com'], ['proxy_port', '123'], ['proxy_user', ''], ['proxy_password', ''], ],
    },
    { label => 'URL with only scheme',
      expected => 'https://www.test.com',
      data => [ ['saas_url', 'fake'], ['saas_token', 'fake'], ['proxy_url', 'https://www.test.com'], ['proxy_port', ''], ['proxy_user', ''], ['proxy_password', ''], ],
    },

);

my $set = create_data_set();
foreach (@test_cases) {
    my $proxy_url = test_proxy_url($set, $_->{data});

    is($proxy_url, $_->{expected}, "Test ".$_->{label});
}

done_testing;
