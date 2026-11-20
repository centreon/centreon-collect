#!/usr/bin/perl

use strict;
use warnings;
use Test2::V0;
use Test2::Plugin::NoWarnings echo => 1;
use File::Temp qw(tempdir);
use FindBin;
use lib "$FindBin::Bin/../../../../../";
use lib "$FindBin::Bin/../../../../../../perl-libs/lib/";
use gorgone::modules::centreon::autodiscovery::services::resources;

# Keeps the error messages so tests can check why a transformation was refused.
package test_logger {
    sub new { return bless({ errors => [] }, shift); }
    sub writeLogError { my ($self, $msg) = @_; push @{$self->{errors}}, $msg; }
    sub writeLogDebug {}
    sub writeLogInfo {}
}

sub change {
    my ($str, $regexp, $replace, $modifier) = @_;
    return { change_str => $str, change_regexp => $regexp, change_replace => $replace, change_modifier => $modifier };
}

# Runs change_vars on one discovered item named $name, as discovery.pm does.
sub run_change_vars {
    my (%args) = @_;

    my $logger = test_logger->new();
    my $svc = {
        service_name => '',
        attributes   => { name => $args{name}, storageid => '35' }
    };
    gorgone::modules::centreon::autodiscovery::services::resources::change_vars(
        discovery_svc      => $svc,
        rule               => { service_display_name => $args{display} // 'Disk-$name$', change => $args{changes} },
        logger             => $logger,
        logger_pre_message => '[test]'
    );
    return ($svc, $logger);
}

sub test_legitimate_changes {
    my @tests = (
        {
            name    => 'no change keeps the display name',
            changes => [],
            item    => '/var/lib',
            result  => 'Disk-/var/lib'
        },
        {
            name    => 'first match only without modifier',
            changes => [ change('@SERVICENAME@', '/', '_', '') ],
            item    => '/var/lib',
            result  => 'Disk-_var/lib'
        },
        {
            name    => "'g' modifier replaces every match",
            changes => [ change('@SERVICENAME@', '/', '_', 'g') ],
            item    => '/var/lib',
            result  => 'Disk-_var_lib'
        },
        {
            name    => "'i' modifier is case insensitive",
            changes => [ change('@SERVICENAME@', '^disk-', 'Fs-', 'i') ],
            item    => '/home',
            result  => 'Fs-/home'
        },
        {
            name    => "'sg' modifiers shipped by plugin packs",
            changes => [ change('@SERVICENAME@', '\s', '-', 'sg') ],
            item    => 'C: Label  System',
            result  => 'Disk-C:-Label--System'
        },
        {
            name    => "'x' modifier ignores whitespace in the regexp",
            changes => [ change('@SERVICENAME@', 'Disk - ', '', 'x') ],
            item    => '/home',
            result  => '/home'
        },
        {
            name    => 'character class with $ and parenthesis, as in plugin packs',
            changes => [ change('@SERVICENAME@', q{[~!$%^&*"|'<>?,()=\s]}, '_', 'g') ],
            item    => 'a$b(c) d',
            result  => 'Disk-a_b_c__d'
        },
        {
            name    => '$1 and ${2} backreferences on an attribute',
            changes => [ change('$name$', '^/(\w+)/(\w+)', '${2}-$1', '') ],
            item    => '/var/lib/docker',
            result  => 'Disk-lib-var/docker'
        },
        {
            name    => '\1 backreference and $& whole match',
            changes => [ change('@SERVICENAME@', '(lib)', '[\1|$&]', 'g') ],
            item    => '/var/lib',
            result  => 'Disk-/var/[lib|lib]'
        },
        {
            name    => 'missing capture group is replaced by nothing',
            changes => [ change('@SERVICENAME@', '^Disk-(/home)(x)?', '$2$1', '') ],
            item    => '/home',
            result  => '/home'
        },
        {
            name    => 'escaped dollar is kept literally',
            changes => [ change('@SERVICENAME@', '^Disk-', '\$1-', '') ],
            item    => '/home',
            result  => '$1-/home'
        },
        {
            name    => 'undefined modifier and replacement remove the match',
            changes => [ change('@SERVICENAME@', '^Disk-', undef, undef) ],
            item    => '/home',
            result  => '/home'
        },
        {
            name    => 'changes are applied in order',
            changes => [ change('@SERVICENAME@', '/', '_', 'g'), change('@SERVICENAME@', '^Disk-_', 'Root-', '') ],
            item    => '/var',
            result  => 'Root-var'
        },
    );
    for my $test (@tests) {
        my ($svc, $logger) = run_change_vars(name => $test->{item}, changes => $test->{changes});
        is($svc->{service_name}, $test->{result}, $test->{name});
        is($logger->{errors}, [], "$test->{name}: no error logged");
    }
}

sub test_injections_are_not_run {
    my $dir = tempdir(CLEANUP => 1);

    my @tests = (
        {
            name   => 'statement appended through the modifier',
            change => sub { change('$storageid$', 'x', 'x', "; system(q{touch $_[0]}) #") },
            error  => qr/change: invalid modifier/
        },
        {
            name   => "'e' modifier",
            change => sub { change('$storageid$', '.+', "system(q{touch $_[0]})", 'e') },
            error  => qr/change: invalid modifier 'e'/
        },
        {
            name   => "'ee' modifier",
            change => sub { change('$storageid$', '.+', "q{system(q{touch $_[0]})}", 'ee') },
            error  => qr/change: invalid modifier 'ee'/
        },
        {
            name   => 'trailing newline after a valid flag',
            change => sub { change('$storageid$', '.+', "system(q{touch $_[0]})", "g\n") },
            # '$' lets the newline through the modifier check, the regexp compilation refuses it
            error  => qr/change: invalid regexp/
        },
        {
            name   => 'interpolated code in the replacement',
            change => sub { change('$storageid$', '.+', "\@{[ system(q{touch $_[0]}) ]}", '') },
            error  => undef
        },
        {
            name   => 'scalar dereference in the replacement',
            change => sub { change('$storageid$', '.+', "\${\\ system(q{touch $_[0]}) }", '') },
            error  => undef
        },
        {
            name   => 'code block in the regexp',
            change => sub { change('$storageid$', "(?{ system(q{touch $_[0]}) })", '', '') },
            error  => qr/change: invalid regexp/
        },
        {
            name   => 'postponed code block in the regexp',
            change => sub { change('$storageid$', "(??{ system(q{touch $_[0]}) })", '', '') },
            error  => qr/change: invalid regexp/
        },
        {
            name   => 'statement appended through the regexp delimiter',
            change => sub { change('$storageid$', "x}{y}; system(q{touch $_[0]}) #", '', '') },
            # perl 5.26 refuses the unescaped brace, perl 5.40 takes it literally: both are safe
            error  => qr/change: invalid regexp/,
            error_optional => 1
        },
        {
            name   => 'statement appended through the replacement delimiter',
            change => sub { change('$storageid$', 'x', "y}; system(q{touch $_[0]}) #", '') },
            error  => undef
        },
    );
    my $i = 0;
    for my $test (@tests) {
        my $marker = "$dir/marker" . $i++;
        my ($svc, $logger) = run_change_vars(name => '/home', changes => [ $test->{change}->($marker) ]);
        ok(!-e $marker, "$test->{name}: no code is run");
        is($svc->{service_name}, 'Disk-/home', "$test->{name}: the service name is not changed");
        if ($test->{error_optional} && !@{$logger->{errors}}) {
            pass("$test->{name}: the text is used as data");
        } elsif (defined($test->{error})) {
            like($logger->{errors}, [ $test->{error} ], "$test->{name}: the change is refused and logged");
        } else {
            is($logger->{errors}, [], "$test->{name}: the text is used as data");
        }
    }
}

sub test_invalid_change_does_not_stop_the_next_ones {
    my ($svc, $logger) = run_change_vars(
        name    => '/home',
        changes => [
            change('@SERVICENAME@', '(', '', ''),
            change('@SERVICENAME@', '^Disk-', 'Fs-', 'e'),
            change('@SERVICENAME@', '^Disk-', 'Fs-', '')
        ]
    );
    is($svc->{service_name}, 'Fs-/home', 'valid change after invalid ones is applied');
    is(scalar(@{$logger->{errors}}), 2, 'each invalid change is logged');
}

test_legitimate_changes();
test_injections_are_not_run();
test_invalid_change_does_not_stop_the_next_ones();
done_testing();
