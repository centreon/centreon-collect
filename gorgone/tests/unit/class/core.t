#!/usr/bin/perl

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
    sub severity {};
    sub new {};
    $INC{ (__PACKAGE__ =~ s{::}{/}rg) . ".pm" } = 1; # this allow the module to be available for other modules anywhere in the code.
}

package main;

use strict;
use warnings;
use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use Data::Dumper;
use FindBin;
use lib "$FindBin::Bin/../../../";
use gorgone::class::script;
use gorgone::class::core;

sub create_data_set {
    my $set = {};
    # as we are in unit test, we can't be sure of our current path, but the tests require that we start from the same directory than the script.
    chdir($FindBin::Bin);
    $set->{logger} = mock 'centreon::common::logger'; # this is from Test2::Tools::Mock, included by Test2::V0
    $set->{vault} = mock 'centreon::common::centreonvault';

    $set->{vault}->override('get_secret' => sub {
        if ($_[1] eq 'secret::hashicorp_vault::SecretPathArg::secretNameFromApiResponse') {
            return 'VaultSentASecret';
        }
       return $_[1];
    }, 'new' => sub {
        return bless({}, 'centreon::common::centreonvault');
    });

    return $set;
}

sub test_configuration_read {
    my $set = shift;
    # let's make a simple object and try to industryalize the yaml read configuration.
    my $gorgone        = gorgone::class::core->new();
    $gorgone->{logger} = $set->{logger};
    $gorgone->{vault} = centreon::common::centreonvault->new();

    my $tests_cases = [
        {
            file => './config_examples/simple_no_recursion/norecursion.yaml',
            expected  => { configuration => { gorgone => {
                key1     => 'a string with all char &é"\'(-è_çà)=!:;,*$^ù%µ£¨/.\e?/§',
                key2     => ["array1", "array2", "array3"],
                TrueVal  => 'true',
                FalseVal => 'false',
                vault  => {
                    badFormat     => 'secret::hashicorp::thereIsOnlyOneColon',
                    correctFormat => 'VaultSentASecret'},

            } } },
            msg  => 'simple configuration without recursion'
        },
        {
            file => './config_examples/include_other_files/main.yaml',
            expected  => { configuration => { gorgone => {
                gorgonecore => { global_variable => "value" }
            } } },
            msg  => 'simple configuration with !include.'
        },
        { # this is a real world exemple with all parameter I could think of. The default configuration don't have all of them.
            # this is more an integration test than a unit test, but allow to test the whole configuration.
            file => './config_examples/centreon-gorgone/config.yaml',
            expected  => require("./config_examples/centreon-gorgone/expectedConfiguration.pl"),
            msg  => 'complete configuration with multiples include and many files.'
        }
    ];

    for my $test (@$tests_cases) {
        my $config = $gorgone->yaml_load_config(
            file   => $test->{file},
            filter => '!($ariane eq "configuration##" || $ariane =~ /^configuration##(?:gorgone|centreon)##/)'
        );
        is($config, $test->{expected}, $test->{msg});
    }

}

sub test_yaml_get_include {
    my $set = shift;
    my $gorgone        = gorgone::class::core->new();
    $gorgone->{logger} = $set->{logger};
    #$gorgone->{vault} = centreon::common::centreonvault->new();
    my @result = $gorgone->yaml_get_include('include' => '*.yaml',
          'current_dir' => './config_examples/include_other_files',
          'filter' => '!($ariane eq "configuration##" || $ariane =~ /^configuration##(?:gorgone|centreon)##/)');
    my @expected = ("./config_examples/include_other_files/./first_module.yaml", "./config_examples/include_other_files/./main.yaml");
    is(\@result, \@expected, 'found both files of the directory');

    my @emptyResult = $gorgone->yaml_get_include('include' => '/notAFile.yaml',
          'current_dir' => './config_examples/include_other_files',
          'filter' => '!($ariane eq "configuration##" || $ariane =~ /^configuration##(?:gorgone|centreon)##/)');
    is(scalar(@emptyResult), 0, 'no file found return empty');
}
sub main {
    my $set = create_data_set();
    test_yaml_get_include($set);
    test_configuration_read($set);

    print "\n";
    done_testing;
}
&main;

