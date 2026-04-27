#!/usr/bin/perl
use strict;
use warnings;
use Test2::V0;
use Data::Dumper;
#use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use FindBin;
use lib "$FindBin::Bin/../../../../../";
use tests::unit::lib::mockCentreonvault;
use gorgone::standard::library;
use gorgone::modules::core::proxy::httpserver;
use tests::unit::lib::mockLogger;
use gorgone::class::db;

sub is_token_ok {
    my $logger = centreon::common::logger->new();
    $logger->severity("debug");
    my $class = bless({
        ws_clients => { 2 => { authorization => "Bearer " } },
        identities => {},
        nodes      => {},
        logger     => $logger,
        config     => { httpserver => { token => 'TheDefaultToken' } },
    },
        "gorgone::modules::core::proxy::httpserver");

    my %input = (data => '[REGISTERNODES] [1~1] [loggingToken] {"nodes":[{"type":"wss","id":"2","identity":"2"}]}', ws_id => "random");
    my @tc = (
        { result => 0, ws => {} },
        { result => 0, ws => undef },
        { result => 0, ws => { 2 => {} } },
        { result => 0, ws => { 2 => { authorization => "" } } },
        { result => 0, ws => { 2 => { authorization => undef} } },
        { result => 0, ws => { 2 => { authorization => "Bearer " } } },
        { result => 0, ws => { 2 => { authorization => "Bearer" } } },
        { result  => 1, ws => { random => { authorization => "Bearer token_from_db" } },
            nodes => { 2 => { token => "token_from_db" } },
            "msg" => "auth success from db token" },
        { result  => 0, ws => { random => { authorization => "Bearer TheDefaultToken" } },
            nodes => { 2 => { token => "token_from_db" } },
            "msg" => "auth fail when db token is present but not used" },
        { result => 1, ws => { random => { authorization => "Bearer TheDefaultToken" } },
            nodes => { 2 => { token => "" } },
            "msg" => "auth success from conf token empty string" },
        { result => 1, ws => { random => { authorization => "Bearer TheDefaultToken" } },
            nodes => { 2 => { token => undef } },
            "msg" => "auth success from conf token undef" },
        { result => 1, ws => { random => { authorization => "Bearer TheDefaultToken" } },
            nodes => { 2 => { token => 0 } },
            "msg" => "auth success from conf token 0" },
        { result => 1, ws => { random => { authorization => "Bearer TheDefaultToken" } },
            nodes => { 2 => {} },
            "msg" => "auth success from conf token empty" },

    );

    for my $t (@tc) {
        $class->{ws_clients} = $t->{ws};
        $class->{nodes} = $t->{nodes};
        is($class->is_token_ok(%input), $t->{result}, $t->{msg} // "no value mean no auth.");
    }
    $class->{ws_clients} = { random => { authorization => "Bearer TheDefaultToken" } } ;
    $class->{nodes} =  { 2 => {} };
    %input = (data => '[REGISTERNODES] [1~1] [loggingToken] {"nodes":[]}', ws_id => "random");

    is($class->is_token_ok(data => '[REGISTERNODES] [1~1] [loggingToken] {"nodes":[]}', ws_id => "random"), 0, "registednodes don't have any nodes info");
}

sub main {
    is_token_ok();
    done_testing();

}
&main;