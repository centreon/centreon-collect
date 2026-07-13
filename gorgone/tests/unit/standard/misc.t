use strict;
use warnings;
use Test2::V0;
use Test2::Tools::Compare qw{is like match};
use Data::Dumper;

use FindBin;
use lib "$FindBin::Bin/../../../";
use gorgone::standard::misc;
use tests::unit::lib::mockLogger;


# in real world one should use execute(), but as many options are not supported by windows_execute,
# the signature is not coherent, and we want to test everything on this unix_execute()
sub test_unix_execute {
    my $mock_logger = mock 'centreon::common::logger'; # this is from Test2::Tools::Mock, included by Test2::V0
    $mock_logger->override('writeLogError' => sub {});
    my $logger = centreon::common::logger->new();

    my @tests = (
        {
            msg    => 'No interpretation, args in command and arguments',
            expect => ' 1:tot 2:o 3:other',
            args   => {
                command                 => q{./showArgs.sh "tot" o},
                no_shell_interpretation => 1,
                arguments               => [ 'other' ],

            }
        },
        {
            msg    => 'No interpretation, args in arguments only',
            expect => ' 1:first 2:second',
            args   => {
                command                 => './showArgs.sh',
                no_shell_interpretation => 1,
                arguments               => [ 'first', "second" ],
            }
        },
        {
            msg    => 'args in command only, no space',
            expect => ' 1:first 2:second 3:third',
            args   => {
                command => './showArgs.sh "first" second "third"',
            }
        },
        {
            msg    => 'args in command only, with space',
            expect => ' 1:fir st 2:second 3:third',
            args   => {
                command => './showArgs.sh "fir st" "second" "third"',
            }
        },
        {
            msg    => 'args in command only, simple quotes',
            expect => ' 1:fir st 2:second 3:third',
            args   => {
                command => "./showArgs.sh 'fir st' 'second' 'third'",
            }
        },
        {
            msg    => 'args in command only, injection with space',
            expect => ' 1:first 2:second 3:third',
            args   => {
                command => "./showArgs.sh \$(echo first second) 'third'",
            }
        },
        {
            msg    => 'args in command only, no injection with space',
            expect => ' 1:$(echo fir"st second) 2:false third',
            args   => {
                command                 => "./showArgs.sh '\$(echo fir\"st second)' 'false third'",
                no_shell_interpretation => 1,
            }
        },
        {
            msg    => 'no interpretation, with space and quotes',
            expect => ' 1:--first=arg 2:"second arg 3:"thirdarg 4:fourth arg',
            args   => {
                command                 => q{./showArgs.sh --first='arg' '"second arg' \"thirdarg},
                arguments               => [ 'fourth arg' ],
                no_shell_interpretation => 1,
            }
        },
        {
            msg    => 'no interpretation, incorrect command',
            expect => 'Error executing the command ./showArgs.sh, does the command require a shell, or is there too many quote ?',
            args   => {
                command                 => q{./showArgs.sh "firstarg},
                no_shell_interpretation => 1,
            }
        },
        {
            msg    => 'no interpretation, incorrect command with arguments',
            expect => 'Error executing the command ./showArgs.sh, does the command require a shell, or is there too many quote ?',
            args   => {
                command                 => q{./showArgs.sh "firstarg},
                arguments               => [ 'second arg' ],
                no_shell_interpretation => 1,
            }
        },
        {
            msg    => 'no interpretation, final space do not add an undef argument',
            expect => ' 1:endwithspace',
            args   => {
                command                 => "./showArgs.sh endwithspace   ",
                no_shell_interpretation => 1,
            }
        },
        {
            msg    => 'no interpretation, only space command return an error',
            expect => 'Error executing the command , does the command require a shell, or is there too many quote ?',
            args   => {
                command                 => "   ",
                no_shell_interpretation => 1,
            }
        }
    );

    for my $test (@tests) {
        my ($status, $output, $status_code) = gorgone::standard::misc::backtick(
            logger    => $logger,
            wait_exit => 1,
            %{$test->{args}},
        );
        is($output, $test->{expect}, $test->{msg});
    }
    # If the binary executed is not found, the command should not be rerun with a shell
    unlink('/tmp/injected_commandUnitTest.txt');
    my ($status, $output, $status_code) = gorgone::standard::misc::backtick(
        logger                    => $logger,
        wait_exit                 => 1,
        command                   => './binary_not_found with args ; echo "injected command from binary_not_found" >> /tmp/injected_commandUnitTest.txt',
        'no_shell_interpretation' => 1,
    );
    # is($status, 127, 'Command not found returns 127');
    ok(!-r '/tmp/injected_commandUnitTest.txt', 'No binary interpretation and no binary does not execute the command, no file created');
}
chdir($FindBin::Bin);
chmod(0755, './showArgs.sh');
test_unix_execute();
done_testing();
