#!/usr/bin/perl
# Lists the APT packages a container.d script must install, from the JSON
# files that configure the container. Core Perl only (JSON::PP), so that the
# image needs no Python.
#
# check_packages.pl plugins <plugins_json_path>
#   plugins.json maps package names to versions. Prints, space-separated, the
#   packages that are not installed or whose installed version does not start
#   with the requested one.
# check_packages.pl custom-deps <custom_deps_json_path>
#   Prints, space-separated, the valid package names of the "apt" list.
#
# Package names are lowercased and checked against Debian naming rules; an
# invalid one is reported on stderr and skipped. Same behaviour as the former
# check_plugins.py and check_custom_deps.py, including the order of the
# output (the order of the file) and the conversion of non-string JSON values
# to text (Python str()).

use strict;
use warnings;
use feature 'unicode_strings';
use JSON::PP ();

binmode STDOUT, ':encoding(UTF-8)';
binmode STDERR, ':encoding(UTF-8)';

my $PKG_RE = qr/^[a-z0-9][a-z0-9+\-.]+$/;

# JSON values are decoded into tagged nodes that keep the order of object
# members: [str => $s], [int => $text], [float => $n], [bool => 0|1],
# [null], [array => [@nodes]], [object => [[$key, $node], ...]].
my $text;

# Never a zero-length match: Perl refuses two in a row at the same position.
sub _ws { $text =~ /\G[ \t\n\r]+/gc }

sub _value {
    _ws();
    if ($text =~ /\G("(?:[^"\\]|\\.)*")/gcs) {
        return [str => JSON::PP->new->allow_nonref->decode($1)];
    }
    if ($text =~ /\G\{/gc) {
        my @members;
        _ws();
        return [object => \@members] if $text =~ /\G\}/gc;
        while (1) {
            my $key = _value();
            die "object key is not a string\n" unless $key->[0] eq 'str';
            _ws();
            $text =~ /\G:/gc or die "':' expected\n";
            push @members, [$key->[1], _value()];
            _ws();
            next if $text =~ /\G,/gc;
            last if $text =~ /\G\}/gc;
            die "',' or '}' expected\n";
        }
        return [object => \@members];
    }
    if ($text =~ /\G\[/gc) {
        my @items;
        _ws();
        return [array => \@items] if $text =~ /\G\]/gc;
        while (1) {
            push @items, _value();
            _ws();
            next if $text =~ /\G,/gc;
            last if $text =~ /\G\]/gc;
            die "',' or ']' expected\n";
        }
        return [array => \@items];
    }
    if ($text =~ /\G(-?(?:0|[1-9][0-9]*))((?:\.[0-9]+)?(?:[eE][-+]?[0-9]+)?)/gc) {
        return $2 eq '' ? [int => $1] : [float => 0 + "$1$2", "$1$2"];
    }
    return [bool => 1] if $text =~ /\Gtrue/gc;
    return [bool => 0] if $text =~ /\Gfalse/gc;
    return ['null'] if $text =~ /\Gnull/gc;
    # Python's json module also accepts these three literals.
    return [float => 9**9**9] if $text =~ /\GInfinity/gc;
    return [float => -9**9**9] if $text =~ /\G-Infinity/gc;
    return [float => -sin(9**9**9)] if $text =~ /\GNaN/gc;
    die "unexpected character\n";
}

# Decodes the file; dies on an unreadable file or invalid JSON.
sub decode_file {
    my ($path) = @_;
    open(my $fh, '<:raw', $path) or die "$path: $!\n";
    local $/;
    my $raw = <$fh>;
    close($fh);
    utf8::decode($raw) or die "$path: invalid UTF-8\n";
    $text = $raw;
    pos($text) = 0;
    my $node = _value();
    _ws();
    die "extra data\n" if pos($text) < length($text);
    return $node;
}

# Last value of a key, as Python's dict keeps it.
sub member {
    my ($object, $key) = @_;
    my $found;
    for my $m (@{$object->[1]}) {
        $found = $m->[1] if $m->[0] eq $key;
    }
    return $found;
}

# Keys in first-seen order, each with its last value (Python dict).
sub members {
    my ($object) = @_;
    my (@keys, %value);
    for my $m (@{$object->[1]}) {
        push @keys, $m->[0] unless exists $value{$m->[0]};
        $value{$m->[0]} = $m->[1];
    }
    return map { [$_, $value{$_}] } @keys;
}

# Python repr() of a float: shortest round-trip digits, scientific notation
# below 1e-4 and from 1e16, ".0" on integral values.
sub py_float {
    my ($x, $literal) = @_;
    return 'nan' if $x != $x;
    return $x > 0 ? 'inf' : '-inf' if $x * 0 != 0;
    # Perl numifies "-0.0" to 0: the sign of a zero comes from the literal.
    my $sign = ($x < 0 || ($x == 0 && ($literal // '') =~ /^-/)) ? '-' : '';
    my $a = abs($x);
    return "${sign}0.0" if $a == 0;
    my ($digits, $exp);
    for my $p (1 .. 17) {
        my $s = sprintf('%.*e', $p - 1, $a);
        next if $p < 17 && $s + 0 != $a;
        ($digits, $exp) = $s =~ /^(\d(?:\.\d+)?)e([-+]\d+)$/;
        last;
    }
    $digits =~ s/\.//;
    $digits =~ s/0+$//;
    $digits = '0' if $digits eq '';
    $exp += 0;
    if ($exp < -4 || $exp >= 16) {
        my $m = substr($digits, 0, 1);
        $m .= '.' . substr($digits, 1) if length($digits) > 1;
        return sprintf('%s%se%s%02d', $sign, $m, $exp < 0 ? '-' : '+', abs($exp));
    }
    if ($exp < 0) {
        return $sign . '0.' . ('0' x (-$exp - 1)) . $digits;
    }
    my $int = substr($digits . ('0' x ($exp + 1)), 0, $exp + 1);
    my $frac = length($digits) > $exp + 1 ? substr($digits, $exp + 1) : '0';
    return "$sign$int.$frac";
}

sub py_repr_str {
    my ($s) = @_;
    my $q = ($s =~ /'/ && $s !~ /"/) ? '"' : "'";
    $s =~ s/\\/\\\\/g;
    $s =~ s/\Q$q\E/\\$q/g;
    $s =~ s/\n/\\n/g;
    $s =~ s/\r/\\r/g;
    $s =~ s/\t/\\t/g;
    $s =~ s/([\x00-\x1f\x7f])/sprintf('\\x%02x', ord($1))/ge;
    return "$q$s$q";
}

# Python str() of a decoded value.
sub py_str {
    my ($node, $nested) = @_;
    my $type = $node->[0];
    return $nested ? py_repr_str($node->[1]) : $node->[1] if $type eq 'str';
    if ($type eq 'int') {
        (my $i = $node->[1]) =~ s/^-0$/0/;
        return $i;
    }
    return py_float($node->[1], $node->[2]) if $type eq 'float';
    return $node->[1] ? 'True' : 'False' if $type eq 'bool';
    return 'None' if $type eq 'null';
    return '[' . join(', ', map { py_str($_, 1) } @{$node->[1]}) . ']'
        if $type eq 'array';
    return '{'
        . join(', ', map { py_repr_str($_->[0]) . ': ' . py_str($_->[1], 1) } members($node))
        . '}';
}

# A value of an unexpected type: Python exited with 1 (uncaught exception).
sub fail {
    print STDERR "$_[0]\n";
    exit 1;
}

# Installed version of a package, empty when dpkg does not know it.
sub installed_version {
    my ($pkg) = @_;
    my $pid = open(my $out, '-|') // die "fork: $!\n";
    if (!$pid) {
        open(STDERR, '>', '/dev/null');
        exec('dpkg-query', '-W', '-f=${Version}', $pkg) or exit 127;
    }
    local $/;
    my $version = <$out> // '';
    my $ok = close($out);
    utf8::decode($version);
    $version =~ s/^\s+|\s+$//g;
    return $ok ? $version : '';
}

sub plugins {
    my ($path) = @_;
    my $plugins = eval { decode_file($path) };
    exit 0 unless $plugins;
    fail('plugins.json: a JSON object is expected') unless $plugins->[0] eq 'object';
    my @to_install;
    for my $member (members($plugins)) {
        my $pkg = lc($member->[0]);
        if ($pkg !~ $PKG_RE) {
            print STDERR "  $pkg: invalid package name, skipping\n";
            next;
        }
        my $ver = py_str($member->[1]);
        my $installed = installed_version($pkg);
        if ($installed eq '') {
            print STDERR "  $pkg: not installed -> queuing\n";
            push @to_install, $pkg;
        } elsif (substr($installed, 0, length($ver)) eq $ver) {
            print STDERR "  $pkg: up-to-date ($installed)\n";
        } else {
            print STDERR "  $pkg: outdated ($installed != $ver) -> queuing\n";
            push @to_install, $pkg;
        }
    }
    print join(' ', @to_install), "\n";
}

sub custom_deps {
    my ($path) = @_;
    my $deps = eval { decode_file($path) };
    if (!$deps) {
        chomp(my $error = $@);
        print STDERR "Failed to read $path: $error\n";
        exit 0;
    }
    fail('custom-deps.json: a JSON object is expected') unless $deps->[0] eq 'object';
    my $apt = member($deps, 'apt') // [array => []];
    my @entries;
    if ($apt->[0] eq 'array') {
        @entries = @{$apt->[1]};
    } elsif ($apt->[0] eq 'object') {
        @entries = map { [str => $_->[0]] } members($apt);
    } elsif ($apt->[0] eq 'str') {
        # Python iterates over the characters of a string.
        @entries = map { [str => $_] } split(//, $apt->[1]);
    } else {
        fail('custom-deps.json: "apt" is not a list');
    }
    my @pkgs;
    for my $entry (@entries) {
        my $pkg = lc(py_str($entry));
        if ($pkg =~ $PKG_RE) {
            push @pkgs, $pkg;
        } else {
            print STDERR "  $pkg: invalid package name, skipping\n";
        }
    }
    print join(' ', @pkgs), "\n";
}

my %modes = ('plugins' => \&plugins, 'custom-deps' => \&custom_deps);
if (@ARGV < 2 || !$modes{$ARGV[0]}) {
    print STDERR "Usage: check_packages.pl plugins|custom-deps <json_path>\n";
    exit 1;
}
$modes{$ARGV[0]}->($ARGV[1]);
