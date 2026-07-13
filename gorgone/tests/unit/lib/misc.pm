package tests::unit::lib::misc;

use strict;
use warnings;

use Exporter;

our @ISA = qw(Exporter);
our @EXPORT_OK = qw(exec_sql db_description);

# Execute all SQL commands contained in $file on the DBI handle $dbh
# Returns an error message on failure, or undef if successful
sub exec_sql($$) {
    my ($dbh, $file) = @_;

    return 'invalid database handle' unless $dbh;
    return "$file does not exist" unless -f $file;
    return "cannot open $file: $!" unless open(my $fic, "<$file");

    foreach my $command (<$fic>) {
        $dbh->do($command) or do {
            close($fic);
            return "sql error: $DBI::errstr";
        }
    }
    close($fic);

    undef
}

# Return a hash containing the database connection information used for tests.
# When the type is "storage", parameters specific to the centreon-storage database are preferred.
# The following environment variables are used: DBTYPE, DBHOST, DBCENTREON, DBUSER, DBPASSWORD.
# For a "storage" database, the following variables are also checked: DBTYPE_STORAGE, DBHOST_STORAGE, DBCENTREON_STORAGE, DBUSER_STORAGE, and DBPASSWORD_STORAGE.
sub db_description {
    my ($type) = @_;

    $type = '' unless $type && lc $type eq 'storage';

    my $ident = $type ? '_STORAGE' : '';
    my $dbdefault = $type ? 'centreon-storage' : 'centreon';

    my $dbtype = $ENV{"DBTYPE$ident"} || $ENV{DBTYPE} || 'mysql';
    my $dbhost = $ENV{"DBHOST$ident"} || $ENV{DBHOST} || 'mariadb';

    my $dbname = $ENV{"DBCENTREON$ident"} || $ENV{DBCENTREON} || $dbdefault;

    my $dbuser = $ENV{"DBUSER$ident"} || $ENV{DBUSER} || 'centreon';
    my $dbpassword = $ENV{"DBPASSWORD$ident"} || $ENV{DBPASSWORD} || 'password';

    return { dsn => "$dbtype:host=$dbhost;dbname=$dbname",
             user => $dbuser,
             password => $dbpassword };
}

1;
