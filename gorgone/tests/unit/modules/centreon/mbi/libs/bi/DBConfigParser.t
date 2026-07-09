#!/usr/bin/perl
use strict;
use warnings;
use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like match};
use File::Temp qw(tempfile);
use FindBin;
use lib "$FindBin::Bin/../../../../../../../";
use gorgone::modules::centreon::mbi::libs::bi::DBConfigParser;

package TestLogger;
sub new { bless {}, shift }
sub writeLog {
    my ($self, $level, $msg) = @_;
    warn "[$level] $msg\n" if $ENV{TEST_VERBOSE};
}
package main;

sub write_profile_xml {
    my ($oda_url) = @_;
    my ($fh, $filename) = tempfile(SUFFIX => '.xml', UNLINK => 1);
    print $fh <<"XML";
<?xml version="1.0" encoding="UTF-8"?>
<DataTools.ServerProfiles>
  <profile autoconnect="No" name="Centreon" providerID="org.eclipse.birt.report.data.oda.jdbc">
    <baseproperties>
      <property name="odaURL" value="$oda_url"/>
      <property name="odaUser" value="centreonbi"/>
      <property name="odaPassword" value="centreonbi"/>
    </baseproperties>
  </profile>
</DataTools.ServerProfiles>
XML
    close $fh;
    return $filename;
}

sub test_parseFile {
    my $parser = gorgone::modules::centreon::mbi::libs::bi::DBConfigParser->new(TestLogger->new);

    my @cases = (
        {
            name => 'plain URL without query params',
            url  => 'jdbc:mariadb://db:3306/centreon',
            db   => 'centreon',
            host => 'db',
            port => '3306',
        },
        {
            name => 'URL with autoReconnect only (legacy default)',
            url  => 'jdbc:mariadb://db:3306/centreon?autoReconnect=true',
            db   => 'centreon',
            host => 'db',
            port => '3306',
        },
        {
            name => 'URL with sslMode only',
            url  => 'jdbc:mariadb://db:3306/centreon?sslMode=trust',
            db   => 'centreon',
            host => 'db',
            port => '3306',
        },
        {
            name => 'URL with autoReconnect and sslMode (centreon-mbi 26.05 production case)',
            url  => 'jdbc:mariadb://db:3306/centreon?autoReconnect=true&amp;sslMode=trust',
            db   => 'centreon',
            host => 'db',
            port => '3306',
        },
        {
            name => 'URL with params in reverse order',
            url  => 'jdbc:mariadb://db:3306/centreon?sslMode=trust&amp;autoReconnect=true',
            db   => 'centreon',
            host => 'db',
            port => '3306',
        },
        {
            name => 'URL without explicit port',
            url  => 'jdbc:mariadb://localhost/centreon_storage?sslMode=trust',
            db   => 'centreon_storage',
            host => 'localhost',
            port => '3306',
        },
        {
            name => 'MySQL connector URL (legacy migration source)',
            url  => 'jdbc:mysql://db:3306/centreon?autoReconnect=true',
            db   => 'centreon',
            host => 'db',
            port => '3306',
        },
    );

    for my $case (@cases) {
        my $file = write_profile_xml($case->{url});
        my $profiles = $parser->parseFile($file);

        is($profiles->{'Centreon_db'},   $case->{db},
            "$case->{name}: db name has no query params");
        is($profiles->{'Centreon_host'}, $case->{host},
            "$case->{name}: host extracted correctly");
        is($profiles->{'Centreon_port'}, $case->{port},
            "$case->{name}: port extracted correctly");
    }
}

sub main {
    test_parseFile();
    done_testing();
}

main;
