#!/usr/bin/perl

package main;

use strict;
use warnings;
use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use Data::Dumper;
use FindBin;
use lib "$FindBin::Bin/../../../";
use tests::unit::lib::mockLogger;
use tests::unit::lib::mockCentreonvault;
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
# check that the argument in argv can have a value from env variable, argv override env.
sub test_new_env_configuration {
    my @argv_backup = @ARGV;
    my @tests = (
        {env => undef, argv=> undef, res => undef},
        {env => "env", argv=> undef, res => "env"},
        {env => "env", argv=> "", res => "env"},
        {env => "env", argv=> "arg", res => "arg"},
        {env => undef, argv=> "arg", res => "arg"},
        );
    my @variables_names = (
        ["config", "config_file"],
        ["vault","vault_config_file"],
        ["logfile", "log_file"],
        ["severity", "severity"]);
    # pub is the public name, used in command line, private is the variable name mapped to it.
    for my $ref (@variables_names) {
        my ($pub, $private) = @$ref;
        for my $t (@tests){
            @ARGV = ();
            @ARGV = ("--$pub" , $t->{argv}) if $t->{argv};
            $ENV{"GORGONE_INIT_" . uc($pub)} = $t->{env};
            # severity is the only argument that have a default value.
            if ($pub eq "severity") {
                if (!defined($t->{res})) {
                    $t->{res} = "info";
                }
                if (!defined($t->{env})) {
                    $t->{env} = "info";
                }
            }

            my $obj = gorgone::class::core->new();
            is($obj->{$private}, $t->{env}, "$pub is taken from env");
            $obj->parse_options();
            is($obj->{$private}, $t->{res}, "$pub is taken from ARGV if needed");
        }
    }
    @ARGV = @argv_backup;
}
sub test_config_from_env {
    $ENV{"GORGONE__GORGONE__MODULES__ACTION__COMMAND_TIMEOUT"} = 5;
    $ENV{"GORGONE__GORGONE__MODULES__ACTION__NEW_ARGUMENT"} = "value";
    $ENV{"GORGONE__GORGONE__MODULES__PROXY__HTTPSERVER__SSL"} = 1;
    $ENV{"GORGONE__GORGONE__MODULES__NEWMODULE__NAME"} = "newmodule";
    $ENV{"GORGONE__GORGONE__MODULES__NEWMODULE__PARAM__SUB_PARAM"} = "new_module_value";
    $ENV{"GORGONE__GORGONE__MODULES__PULLWSS_TOKEN"} = "token_from_long_env_variable";
    $ENV{GORGONE_TOKEN} = "new_token!";
    my $gorgone = gorgone::class::core->new();
    $gorgone->{config} = $gorgone->yaml_load_config(
            file   => './config_examples/centreon-gorgone/config.yaml',
            filter => '!($ariane eq "configuration##" || $ariane =~ /^configuration##(?:gorgone|centreon)##/)'
        );
    $gorgone->load_env_config();
    my $modules = $gorgone->{config}->{configuration}->{gorgone}->{modules};
    my $action_module = undef;
    my $proxy_module = undef;
    my $new_module = undef;
    my $pullwss_module = undef;

    for my $module (@$modules){
        if ($module->{name} eq "action"){
        $action_module = $module;
        }
        if ($module->{name} eq "proxy"){
            $proxy_module = $module;
        }
        if ($module->{name} eq "newmodule"){
            $new_module = $module;
        }
        if ($module->{name} eq "pullwss"){
            $pullwss_module = $module;
        }
    }

    isnt($action_module, undef, "action module should exist");
        print(Dumper($action_module));

    is($action_module->{command_timeout}, 5, "env variable should override yaml config");
    is($action_module->{new_argument}, "value", "new variable creation is possible");

    isnt($proxy_module, undef, "proxy module should exist");
    is($proxy_module->{httpserver}->{ssl}, 1, "set sub module configuration");

    isnt($new_module, undef, "new module should exist");
    is($new_module->{param}->{sub_param}, "new_module_value", "set sub module configuration");

    isnt($pullwss_module, undef, "pullwss module should exist");
    is($pullwss_module->{token}, "new_token!", "shorter option name override longer one");
}
sub main {
    my $set = create_data_set();
    test_yaml_get_include($set);
    test_configuration_read($set);
    test_new_env_configuration();
    test_config_from_env();

    print "\n";
    done_testing;
}
&main;

