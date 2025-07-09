#!/usr/bin/perl
use strict;
use warnings;
use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use FindBin;
use lib "$FindBin::Bin/../../../../../../";
use gorgone::modules::centreon::mbi::etl::class;
sub main {
        eval {
            gorgone::modules::centreon::mbi::etl::class::db_parse_xml(undef, 'file', 'FileThatDoesNotExist.xml');

        };
        if ($@) {
            like($@, qr/FileThatDoesNotExist.xml/, 'FileThatDoesNotExist.xml does not exist');
        }else {
            fail('FileThatDoesNotExist.xml should not exist and send an error.');
        }
    done_testing();
}
main;
