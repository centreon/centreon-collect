#!/usr/bin/perl

use strict;
use warnings;

my $d = time();
my $dd = localtime();
{
    use integer;
    $d = ($d + 3 * $ARGV[0]) & 0x1ff;
}

if ($#ARGV ne 0) {
    die "The script must be used with one integer argument\n";
}

if ($ARGV[0] eq 0) {
  printf("Host check $dd");
}
else {
  $d /= ($ARGV[0] + 1);
  my $w = 300 / ($ARGV[0] + 1);
  my $c = 400 / ($ARGV[0] + 1);
  printf("Test check $ARGV[0] | metric=%.2f;%.2f;%.2f\n", $d, $w, $c);
  if ($d ge $c) {
      exit 2;
  } elsif ($d ge $w) {
      exit 1;
  }
}
exit 0;
