#!/usr/bin/perl
use strict;
use warnings;
use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use File::Temp qw(tempfile);
use FindBin;
use lib "$FindBin::Bin/../../../../../../";
use gorgone::modules::centreon::mbi::etl::class;

sub write_profile_xml {
    my ($oda_url_centreon, $oda_url_censtorage) = @_;
    my ($fh, $filename) = tempfile(SUFFIX => '.xml', UNLINK => 1);
    print $fh <<"XML";
<?xml version="1.0" encoding="UTF-8"?>
<DataTools.ServerProfiles>
  <profile autoconnect="No" name="Centreon" providerID="org.eclipse.birt.report.data.oda.jdbc">
    <baseproperties>
      <property name="odaURL" value="$oda_url_centreon"/>
      <property name="odaUser" value="centreonbi"/>
      <property name="odaPassword" value="centreonbi"/>
    </baseproperties>
  </profile>
  <profile autoconnect="No" name="Censtorage" providerID="org.eclipse.birt.report.data.oda.jdbc">
    <baseproperties>
      <property name="odaURL" value="$oda_url_censtorage"/>
      <property name="odaUser" value="centreonbi"/>
      <property name="odaPassword" value="centreonbi"/>
    </baseproperties>
  </profile>
</DataTools.ServerProfiles>
XML
    close $fh;
    return $filename;
}

sub test_missing_file {
    eval {
        gorgone::modules::centreon::mbi::etl::class::db_parse_xml(
            undef, file => 'FileThatDoesNotExist.xml'
        );
    };
    if ($@) {
        like($@, qr/FileThatDoesNotExist.xml/, 'FileThatDoesNotExist.xml does not exist');
    } else {
        fail('FileThatDoesNotExist.xml should not exist and send an error.');
    }
}

sub test_db_parse_xml {
    my @cases = (
        {
            name        => 'plain URL without query params',
            url         => 'jdbc:mariadb://db:3306/centreon',
            storage_url => 'jdbc:mariadb://db:3306/centreon_storage',
            host        => 'db',
            port        => '3306',
            db          => 'centreon',
            storage_db  => 'centreon_storage',
        },
        {
            name        => 'URL with autoReconnect only (legacy default)',
            url         => 'jdbc:mariadb://db:3306/centreon?autoReconnect=true',
            storage_url => 'jdbc:mariadb://db:3306/centreon_storage?autoReconnect=true',
            host        => 'db',
            port        => '3306',
            db          => 'centreon',
            storage_db  => 'centreon_storage',
        },
        {
            name        => 'URL with sslMode only',
            url         => 'jdbc:mariadb://db:3306/centreon?sslMode=trust',
            storage_url => 'jdbc:mariadb://db:3306/centreon_storage?sslMode=trust',
            host        => 'db',
            port        => '3306',
            db          => 'centreon',
            storage_db  => 'centreon_storage',
        },
        {
            name        => 'URL with autoReconnect and sslMode (centreon-mbi 26.05 production case)',
            url         => 'jdbc:mariadb://db:3306/centreon?autoReconnect=true&amp;sslMode=trust',
            storage_url => 'jdbc:mariadb://db:3306/centreon_storage?autoReconnect=true&amp;sslMode=trust',
            host        => 'db',
            port        => '3306',
            db          => 'centreon',
            storage_db  => 'centreon_storage',
        },
        {
            name        => 'URL with params in reverse order',
            url         => 'jdbc:mariadb://db:3306/centreon?sslMode=trust&amp;autoReconnect=true',
            storage_url => 'jdbc:mariadb://db:3306/centreon_storage?sslMode=trust&amp;autoReconnect=true',
            host        => 'db',
            port        => '3306',
            db          => 'centreon',
            storage_db  => 'centreon_storage',
        },
        {
            name        => 'MySQL connector URL (legacy migration source)',
            url         => 'jdbc:mysql://db:3306/centreon?autoReconnect=true',
            storage_url => 'jdbc:mysql://db:3306/centreon_storage?autoReconnect=true',
            host        => 'db',
            port        => '3306',
            db          => 'centreon',
            storage_db  => 'centreon_storage',
        },
    );

    for my $case (@cases) {
        my $file = write_profile_xml($case->{url}, $case->{storage_url});
        my $dbcon = gorgone::modules::centreon::mbi::etl::class::db_parse_xml(
            undef, file => $file
        );
        is($dbcon->{centreon}->{db}, $case->{db},
            "$case->{name}: centreon db name has no query params");
        is($dbcon->{centreon}->{host}, $case->{host},
            "$case->{name}: centreon host extracted correctly");
        is($dbcon->{centreon}->{port}, $case->{port},
            "$case->{name}: centreon port extracted correctly");
        is($dbcon->{centstorage}->{db}, $case->{storage_db},
            "$case->{name}: centstorage db name has no query params");
    }
}

sub main {
    test_missing_file();
    test_db_parse_xml();
    done_testing();
}

main;
