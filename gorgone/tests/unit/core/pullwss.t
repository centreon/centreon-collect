#!/usr/bin/perl
use strict;
use warnings;
use Test2::V0;
use Test2::Tools::Compare qw{is check_isa like match};
use FindBin;
use lib "$FindBin::Bin/../../../";
use gorgone::modules::core::pullwss::class;
use gorgone::class::module;
use centreon::common::logger; # used to

sub test_transmit_back{
    my $mock = mock_send_message(qr/\[SETLOGS\] \[token\] \[\] (.*)/,[10, 11,12,13], [20, 21,22,23], [30, 31,32,33]);
    my $logger = centreon::common::logger->new();
    my $gorgone = gorgone::modules::core::pullwss::class->new(logger => $logger);
    $gorgone->{logger} = $logger;
    # this configuration dictate how many log peer message will be sent, but it's compatbilising only the "data" field.
    # changing this value will change the number of message sent to the remote node.
    $gorgone->{config_core}->{gorgonecore}->{external_pullwss_com_msg_size} = 490;
    is($gorgone, check_isa('gorgone::modules::core::pullwss::class'), 'new object created');
    my $string = '[ACK] [token] {"code":0,"data":{"action":"getlog","result":[{"instant":0,"token":"0lYU5","id":"10","code":2,"etime":1744038787.85076,"ctime":1744038787.85076,"data":"This is test 10. w77eS7lioLxNRgNUfY8mQEoRbgQIstmSwHbiJch6juirHnMqDI54e1PeYe0LiHqTAEZ1bhkaeGu70k1pzw10CZc1focZqbyPUamU"},{"data":"This is test 11. d6CQGUjf1JBZueBIds9XhO2K2nFK3t8Rc0O4c2Ty3OUUSAmeYqbegPjtUG4jHQVa4XH8gaJUSgcKx71qhXQcQVda4onF6dm36bK7","ctime":1744038787.85086,"etime":1744038787.85086,"code":2,"id":"11","token":"0lYU5","instant":0},{"instant":0,"id":"12","token":"0lYU5","etime":1744038787.85096,"code":2,"data":"This is test 12. LACUEPnyD76WGrpuhwdP48BEAwEXF14F7LlpBsvsMKceLChoCtUlXQUUwhvNihELa3fjRv475QtA1DLj1G9x9UAB6r2PZ3fAtsG3","ctime":1744038787.85096},{"id":"13","token":"0lYU5","instant":0,"data":"This is test 13. jjjyuKcqXPJN1NtiDYRnIqijxfaSZSkH8ocN82HuBcVB3AVNgapOAdJJxZcqwOnOMywl71q8DJXh8wA7j3ODDvsr1Jye8RJK3t3Q","ctime":1744038787.85106,"etime":1744038787.85106,"code":2},{"instant":0,"token":"0lYU5","id":"20","code":2,"etime":1744038787.85076,"ctime":1744038787.85076,"data":"This is test 20. w77eS7lioLxNRgNUfY8mQEoRbgQIstmSwHbiJch6juirHnMqDI54e1PeYe0LiHqTAEZ1bhkaeGu70k1pzw10CZc1focZqbyPUamU"},{"data":"This is test 21. d6CQGUjf1JBZueBIds9XhO2K2nFK3t8Rc0O4c2Ty3OUUSAmeYqbegPjtUG4jHQVa4XH8gaJUSgcKx71qhXQcQVda4onF6dm36bK7","ctime":1744038787.85086,"etime":1744038787.85086,"code":2,"id":"21","token":"0lYU5","instant":0},{"instant":0,"id":"22","token":"0lYU5","etime":1744038787.85096,"code":2,"data":"This is test 22. LACUEPnyD76WGrpuhwdP48BEAwEXF14F7LlpBsvsMKceLChoCtUlXQUUwhvNihELa3fjRv475QtA1DLj1G9x9UAB6r2PZ3fAtsG3","ctime":1744038787.85096},{"id":"23","token":"0lYU5","instant":0,"data":"This is test 23. jjjyuKcqXPJN1NtiDYRnIqijxfaSZSkH8ocN82HuBcVB3AVNgapOAdJJxZcqwOnOMywl71q8DJXh8wA7j3ODDvsr1Jye8RJK3t3Q","ctime":1744038787.85106,"etime":1744038787.85106,"code":2}, {"instant":0,"token":"0lYU5","id":"30","code":2,"etime":1744038787.85076,"ctime":1744038787.85076,"data":"This is test 30. w77eS7lioLxNRgNUfY8mQEoRbgQIstmSwHbiJch6juirHnMqDI54e1PeYe0LiHqTAEZ1bhkaeGu70k1pzw10CZc1focZqbyPUamU"},{"data":"This is test 31. d6CQGUjf1JBZueBIds9XhO2K2nFK3t8Rc0O4c2Ty3OUUSAmeYqbegPjtUG4jHQVa4XH8gaJUSgcKx71qhXQcQVda4onF6dm36bK7","ctime":1744038787.85086,"etime":1744038787.85086,"code":2,"id":"31","token":"0lYU5","instant":0},{"instant":0,"id":"32","token":"0lYU5","etime":1744038787.85096,"code":2,"data":"This is test 32. LACUEPnyD76WGrpuhwdP48BEAwEXF14F7LlpBsvsMKceLChoCtUlXQUUwhvNihELa3fjRv475QtA1DLj1G9x9UAB6r2PZ3fAtsG3","ctime":1744038787.85096},{"id":"33","token":"0lYU5","instant":0,"data":"This is test 33. jjjyuKcqXPJN1NtiDYRnIqijxfaSZSkH8ocN82HuBcVB3AVNgapOAdJJxZcqwOnOMywl71q8DJXh8wA7j3ODDvsr1Jye8RJK3t3Q","ctime":1744038787.85106,"etime":1744038787.85106,"code":2}]}}';
    $gorgone->transmit_back(message => \$string);
    my $track = $mock->sub_tracking;
    is(scalar(@{$track->{send_message}}), 3, 'send_message was called 3 times');
}
# for now we don't care about what is logged and what is not.
sub mock_logger {
    my $mock = mock 'centreon::common::logger';
    $mock->override('writeLogError' => sub { },
        'writeLogDebug' => sub { },
        'writeLogInfo' => sub { });
    return $mock;
}
# pullwss mother class is gorgone::class::module, so we need to mock to mock the new() to not start zmq and logger complex behaviour.
sub mock_class_module {
    my $mock = mock 'gorgone::class::module';
    $mock->override('new' => sub{
        return {};
    });
    return $mock;
}

# for now this simple test only check the id of each message is correctly set.
# It does not check the content of the message. or any other property of the log.
# send_message is called by transmit_back() for each message to be sent to a remote node.
sub mock_send_message {
    # expect a regexp as first parameter to disect the message and get the json as $1.
    # nexts arguments are a list of expected log id.
    my $regex = shift;
    my @mock_list = @_;
    my $mock = mock 'gorgone::modules::core::pullwss::class'; # is from Test2::Tools::Mock, included by Test2::V0
    $mock->track(1); # allow to track all call to each mocked sub, so we can check they where called the correct amount of time later.
    $mock->override('send_message' => sub {
        is($_[1], 'message', '[send_message] called with a hash as parameter.');
        is(scalar(@_), 3, '[send_message] only 2 parameter.');
        my $received = $_[2];

        like($received, $regex, '[send_message] message match regex.');
        $received =~ $regex;
        my $json;
        eval {
            $json = JSON::XS->new->decode($1);
        };
        if ($@) {
            fail("error decoding message json : $@");
        }
        my $logs_id = shift(@mock_list);
        is($json->{data}->{result}, E(), "message contain some logs.");
        foreach my $log (@{$json->{data}->{result}}){
            my $expected_id = shift(@$logs_id);
            is($log->{id}, $expected_id, "message contain id $expected_id.");
        }

    }, 'set_signal_handlers' => sub{
        return;
    });
    return $mock;
}

sub main {
    my $mocks = {
        'centreon::common::logger' => mock_logger(),
        'gorgone::class::module' => mock_class_module(),
    };
    test_transmit_back();

    done_testing();
}
main;

