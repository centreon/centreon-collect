#!/usr/bin/perl

use strict;
use warnings;
use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use FindBin;
use lib "$FindBin::Bin/../../../../../";
use gorgone::modules::core::action::class;
use tests::unit::lib::mockLogger;

sub main {
    my $mock_logger = mock 'centreon::common::logger'; # this is from Test2::Tools::Mock, included by Test2::V0

    
    # The writeLogError function is redefined to capture log messages into $output
    my $is_debug = 0;
    my $output = '';
    $mock_logger->override('writeLogError' => sub {
        $output .= "@_\n";
    });
    # is_debug function is redefined to control the debug mode using the $is_debug variable
    $mock_logger->set('is_debug' => sub {
        return $is_debug;
    });

    my %options = (
        logger  => $mock_logger->class,
        module_id => 'core',
        config => {
            whitelist_cmds => 1,

            # Our allowed commands for testing
            allowed_cmds => [ 'ls', 'ls --options' ],
        },
        config_core => { gorgonecore => { internal_com_crypt => 0 } },
    );

    my $gorgone = gorgone::modules::core::action::class->new(%options);

    # Check that in all use cases parameters are only shown in debug mode

    $options{data}->{content} = { command => 'pwd --login=login --password=pAs$W@rd' };
    ($is_debug = 0, $output = '');
    $gorgone->action_actionengine(%options);
    ok($output =~ /command not allowed/ && $output =~ /pwd \.\.\./ && $output !~ /--password/, '(action_actionengine) disallowed command does not display parameters');

    ($is_debug = 1, $output = '');
    $gorgone->action_actionengine(%options);
    ok($output =~ /command not allowed/ && $output =~ /--password/ && $output !~ /\.\.\./ , '(action_actionengine) in debug mode disallowed command displays parameters');
    
    $options{data}->{content} = { command => 'ls --secret=toto' };
    ($is_debug = 0, $output = '');
    $gorgone->action_actionengine(%options);
    ok($output eq "", '(action_actionengine) allowed command display nothing');

    ($is_debug = 1, $output = '');
    $gorgone->action_actionengine(%options);
    ok($output eq "", '(action_actionengine) in debug mode allowed command display nothing');


    $options{data}->{content} = [ { command => 'ls --secret=toto' }, { command => 'pwd --login=login --password=pAs$W@rd' }, ];
    ($is_debug = 0, $output = '');
    $gorgone->action_command(%options);
    ok($output =~ /command not allowed/ && $output =~ /pwd \.\.\./ && $output !~ /--secret/ , '(action_command) disallowed commands does not display parameters');

    ($is_debug = 1, $output = '');
    $gorgone->action_command(%options);
    ok($output =~ /command not allowed/ && $output =~ /--password/ && $output !~ /pwd \.\.\./ && $output !~ /--secret/ , '(action_command) in debug mode disallowed commands does not display parameters');

    $options{data}->{content} = [ { command => 'ls --secret=toto' }, { command => 'ls --options=toto' }, ];
    ($is_debug = 0, $output = '');
    $gorgone->action_command(%options);
    ok($output eq "" , '(action_command) allowed commands display nothing');

    ($is_debug = 1, $output = '');
    $gorgone->action_command(%options);
    ok($output eq "" , '(action_command) in debug mode allowed commands display nothing');

    done_testing();
}
main;
