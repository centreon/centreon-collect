/*
** Copyright 2011-2014 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
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
  "$| = 1;\n" \
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
  "  $sub =~ s/__END__/\\;}\n__END__/;\n" \
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
  "}\n\n";
