/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Connector Perl.
**
** Centreon Connector Perl is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector Perl is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector Perl. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/connector/perl/embedded_perl.hh"

char const* const com::centreon::connector::perl::embedded_perl::_script =
  "#!/usr/bin/perl\n" \
  "\n" \
  "package Embed::Persistent;\n" \
  "\n" \
  "use Text::ParseWords qw(parse_line);\n" \
  "\n" \
  "our %Cache;\n" \
  "\n" \
  "use constant MTIME_IDX  => 0;\n" \
  "use constant HANDLE_IDX => 1;\n" \
  "\n" \
  "sub valid_package_name {\n" \
  "  my ($string) = @_;\n" \
  "  # First pass.\n" \
  "  $string =~ s/([^A-Za-z0-9\\/])/sprintf(\"_%2x\", unpack(\"C\", $1))/eg;\n" \
  "  # Second pass only for words starting with a digit.\n" \
  "  $string =~ s|/(\\d)|sprintf(\"/_%2x\", unpack(\"C\", $1))|eg;\n" \
  "  # Dress it up as a real package name.\n" \
  "  $string =~ s|/|::|g;\n" \
  "  return \"Embed\" . $string;\n" \
  "}\n" \
  "\n" \
  "sub eval_file {\n" \
  "  my ($filename) = @_;\n" \
  "  my $mtime = -M $filename;\n" \
  "  # Is plugin already compiled ?\n" \
  "  if (exists($Cache{$filename})\n" \
  "      && $Cache{$filename}[MTIME_IDX]\n" \
  "      && $Cache{$filename}[MTIME_IDX] <= $mtime) {\n" \
  "      return $Cache{$filename}[HANDLE_IDX];\n" \
  "  }\n" \
  "\n" \
  "  # Read Perl script.\n" \
  "  my $package = valid_package_name($filename);\n" \
  "  open(my $fh, \"<\", $filename)\n" \
  "    or die \"failed to open Perl file '$filename': $!\";\n" \
  "  my $sub;\n" \
  "  sysread $fh, $sub, -s $fh;\n" \
  "  close $fh;\n" \
  "\n" \
  "  # Wrap the code into a subroutine.\n" \
  "  my $hndlr = <<EOSUB;\n" \
  "package $package;\n" \
  "sub subroutine {\n" \
  "  \\@ARGV = \\@_;\n" \
  "  local \\$^W = 1;\n" \
  "  $sub\n" \
  "}\n" \
  "EOSUB\n" \
  "\n" \
  "  # Ensure modified Perl plugins get recached properly.\n" \
  "  no strict 'refs';\n" \
  "  undef %{$package.'::'};\n" \
  "  use strict 'refs';\n" \
  "\n" \
  "  # Compile.\n" \
  "  eval $hndlr;\n" \
  "  if ($@) {\n" \
  "    chomp($@);\n" \
  "    die \"syntax error in '$filename': $@\";\n" \
  "  }\n" \
  "\n" \
  "  # Add script to cache.\n" \
  "  $Cache{$filename}[MTIME_IDX] = $mtime;\n" \
  "  no strict 'refs';\n" \
  "  return $Cache{$filename}[HANDLE_IDX] = *{ $package . '::subroutine' }{CODE} ;\n" \
  "}\n" \
  "\n" \
  "sub run_file {\n" \
  "  # Fetch arguments.\n" \
  "  my ($filename, $handle, $args) = @_;\n" \
  "\n" \
  "  # Parse arguments.\n" \
  "  my @parsed_args = (\"$filename\");\n" \
  "  push(@parsed_args, parse_line('\\s+', 0, $args));\n" \
  "\n" \
  "  # Run subroutine.\n" \
  "  my $res;\n" \
  "  eval { $res = $handle->(@parsed_args) };\n" \
  "  if ($@) {\n" \
  "    chomp($@);\n" \
  "    die \"could not run '$filename': $@\";\n" \
  "  }\n" \
  "  return ($res);\n" \
  "}\n";
