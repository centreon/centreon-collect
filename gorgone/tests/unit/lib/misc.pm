package tests::unit::lib::misc;

use strict;
use warnings;

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

1;
