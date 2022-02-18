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

my $status = -1;

if ($ARGV[0] eq 0) {
  printf("Host check $dd");
}
else {
  if (open(FH, '<', "/tmp/states")) {
    while (<FH>) {
      if (/$ARGV[0]=>(.*)/) {
        $status = $1;
        chomp $status;
      }
    }
    close FH;
  }

  $d /= ($ARGV[0] + 1);
  my $w = 300 / ($ARGV[0] + 1);
  my $c = 400 / ($ARGV[0] + 1);
  if ($status == 0) {
    $d = $w / 2;
  } elsif ($status == 1) {
    $d = ($w + $c) / 2;
  } elsif ($status == 2) {
    $d = 2 * $c;
  } else {
    if ($d > $c) {
      $status = 2;
    } elsif ($d > $w) {
      $status = 1;
    } else {
      $status = 0;
    }
  }
  printf("Test check $ARGV[0] | metric=%.2f;%.2f;%.2f\n", $d, $w, $c);
  exit $status;
}
exit 0;
