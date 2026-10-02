#!/usr/bin/perl

use strict;
use warnings;

use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use Test2::Tools::Compare qw{is like};
use FindBin;
use lib "$FindBin::Bin/../../../../../../../";
use tests::unit::lib::mockLogger;
use tests::unit::lib::misc qw(db_description);
use gorgone::class::db;
use gorgone::modules::centreon::mbi::libs::Messages;
use gorgone::modules::centreon::mbi::libs::bi::MySQLTables;

# This test uses the MySQL database created for Gorgone robot tests.
sub test_getCollationRealignStatements {
    my $logger = mock('centreon::common::logger');
    my $db = gorgone::class::db->new(
        type => 'mysql',
        force => 2,
        logger => $logger->class,
        die => 1,
        %{db_description('storage')}
    );

    is($db, D(), 'BI storage database object should not be undef.');
    ok($db->connect() == 0, 'trying to connect to BI storage database.');

    return unless $db->{instance};

    my $dbh = $db->{instance};
    my $tables = gorgone::modules::centreon::mbi::libs::bi::MySQLTables->new(
        gorgone::modules::centreon::mbi::libs::Messages->new(),
        $db
    );

    my $deviantColumns = 'mod_bi_ut_deviant_columns';
    my $deviantDefault = 'mod_bi_ut_deviant_default';
    my $mixedCharsets = 'mod_bi_ut_mixed_charsets';
    my $transientPrefixed = 'mod_bi_tmp_ut_transient';
    my $transientSuffixed = 'mod_bi_ut_transient_tmp';
    my @fixtures = ($deviantColumns, $deviantDefault, $mixedCharsets, $transientPrefixed, $transientSuffixed);

    foreach my $table (@fixtures) {
        $dbh->do("DROP TABLE IF EXISTS `$table`") or die $dbh->errstr;
    }

    # a utf8mb3 column outside utf8mb3_general_ci, nothing in another character set: convertible.
    $dbh->do(
        "CREATE TABLE `$deviantColumns` (`id` int NOT NULL,"
        . " `hg_name` varchar(255) CHARACTER SET utf8mb3 COLLATE utf8mb3_bin)"
        . " ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
    ) or die $dbh->errstr;
    # columns already correct, only the table default deviates: realigned without a rebuild.
    $dbh->do(
        "CREATE TABLE `$deviantDefault` (`id` int NOT NULL,"
        . " `hg_name` varchar(255) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci)"
        . " ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
    ) or die $dbh->errstr;
    # a deviating utf8mb3 column next to a utf8mb4 one, and a deviating default as well: the whole
    # table must be left alone, converting it would transcode the utf8mb4 column.
    $dbh->do(
        "CREATE TABLE `$mixedCharsets` (`id` int NOT NULL,"
        . " `hg_name` varchar(255) CHARACTER SET utf8mb3 COLLATE utf8mb3_bin,"
        . " `note` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci)"
        . " ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
    ) or die $dbh->errstr;
    # transient tables of the ETL, deviating as well: recreated on every run, so never realigned.
    foreach my $table ($transientPrefixed, $transientSuffixed) {
        $dbh->do(
            "CREATE TABLE `$table` (`id` int NOT NULL,"
            . " `hg_name` varchar(255) CHARACTER SET utf8mb3 COLLATE utf8mb3_bin)"
            . " ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
        ) or die $dbh->errstr;
    }

    my ($statements, $warnings) = $tables->getCollationRealignStatements();

    # Other test files share this database, so only the tables created above are asserted on.
    my @columnStatements = grep { $_->[1] =~ /`\Q$deviantColumns\E`/ } @$statements;
    is(scalar(@columnStatements), 1, 'a table with a deviating column should get one statement.');
    like(
        $columnStatements[0]->[1],
        qr/CONVERT TO CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci/,
        'the columns of a deviating table should be converted.'
    );

    my @defaultStatements = grep { $_->[1] =~ /`\Q$deviantDefault\E`/ } @$statements;
    is(scalar(@defaultStatements), 1, 'a table whose default collation deviates should get one statement.');
    like(
        $defaultStatements[0]->[1],
        qr/DEFAULT CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci/,
        'a deviating default collation should be realigned without rebuilding the table.'
    );

    is(
        scalar(grep { $_->[1] =~ /`\Q$mixedCharsets\E`/ } @$statements),
        0,
        'a table mixing character sets should get no statement at all.'
    );
    is(
        scalar(grep { /\Q$mixedCharsets\E/ } @$warnings),
        1,
        'a table mixing character sets should be reported.'
    );

    is(
        scalar(grep { $_->[1] =~ /`\Q$transientPrefixed\E`|`\Q$transientSuffixed\E`/ } @$statements),
        0,
        'the transient tables of the ETL should be left out.'
    );

    foreach my $statement (@columnStatements, @defaultStatements) {
        $dbh->do($statement->[1]) or die $dbh->errstr;
    }

    my ($collation) = $dbh->selectrow_array(
        "SELECT COLLATION_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE()"
        . " AND TABLE_NAME = '$deviantColumns' AND COLUMN_NAME = 'hg_name'"
    );
    is($collation, 'utf8mb3_general_ci', 'the deviating column should end up in utf8mb3_general_ci.');

    foreach my $table ($deviantColumns, $deviantDefault) {
        my ($tableCollation) = $dbh->selectrow_array(
            "SELECT TABLE_COLLATION FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE()"
            . " AND TABLE_NAME = '$table'"
        );
        is($tableCollation, 'utf8mb3_general_ci', "the default collation of $table should be realigned.");
    }

    my ($mixedCollation) = $dbh->selectrow_array(
        "SELECT TABLE_COLLATION FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE()"
        . " AND TABLE_NAME = '$mixedCharsets'"
    );
    is($mixedCollation, 'utf8mb3_bin', 'a reported table should be left exactly as it is, default collation included.');

    ($statements, $warnings) = $tables->getCollationRealignStatements();
    is(
        scalar(grep { $_->[1] =~ /`\Q$deviantColumns\E`|`\Q$deviantDefault\E`/ } @$statements),
        0,
        'a realigned table should not be selected any more.'
    );

    foreach my $table (@fixtures) {
        $dbh->do("DROP TABLE IF EXISTS `$table`") or die $dbh->errstr;
    }
}

# Without partitions, the table is dropped, created and indexed. A CREATE
# failing because of an orphaned tablespace must report the hint and not index
# the table.
sub test_emptyTableForRebuild {
    my $structure = "CREATE TABLE `mod_bi_ut_rebuild` (\n  `time_id` int(11) NOT NULL\n) ENGINE=InnoDB";
    my $drop = 'DROP TABLE IF EXISTS `mod_bi_ut_rebuild`';
    my $index = 'ALTER TABLE `mod_bi_ut_rebuild` ADD INDEX `idx_mod_bi_ut_rebuild_time_id` (`time_id`)';

    for my $fail (0, 1) {
        my @queries;
        my $db = mock { die => 1 } => (
            add => [
                query => sub {
                    my ($self, $options) = @_;

                    push @queries, $options->{query};
                    die "SQL error: Tablespace '`centreon_storage`.`mod_bi_ut_rebuild`' exists. (caller: x:y:1)\n"
                        . "Query: $options->{query}\n"
                        if ($fail && $options->{query} eq $structure);
                    return 1;
                }
            ]
        );
        my $tables = gorgone::modules::centreon::mbi::libs::bi::MySQLTables->new(
            gorgone::modules::centreon::mbi::libs::Messages->new(),
            $db
        );

        if (!$fail) {
            ok(lives { $tables->emptyTableForRebuild('mod_bi_ut_rebuild', $structure, 'time_id') },
                'the table should be rebuilt.');
            is(\@queries, [$drop, $structure, $index], 'the table should be dropped, created and indexed.');
        } else {
            like(dies { $tables->emptyTableForRebuild('mod_bi_ut_rebuild', $structure, 'time_id') },
                qr/Cannot create table `mod_bi_ut_rebuild`: an orphaned InnoDB tablespace/,
                'an orphaned tablespace should be reported.');
            is(\@queries, [$drop, $structure], 'the table should not be indexed after a failure.');
        }
    }
}

sub main {
    test_emptyTableForRebuild();
    test_getCollationRealignStatements();
    done_testing();
}

main
