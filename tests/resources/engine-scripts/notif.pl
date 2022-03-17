#!/usr/bin/perl

use strict;
use warnings;

my $d = `date +%s`;
chomp $d;

open(FH, ">>", "/tmp/notif") or die "Unable to open /tmp/notif";
print FH "$d => notif\n";
close(FH);

exit 0;
