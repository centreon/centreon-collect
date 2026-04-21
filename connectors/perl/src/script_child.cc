/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include "connectors/perl/src/perl_connector.pb.h"

#include <EXTERN.h>
#include <perl.h>
#include <boost/asio/writable_pipe.hpp>
#include <boost/system/detail/error_code.hpp>
#include <chrono>
#include <memory>
#include <system_error>

#include "com/centreon/connector/perl/script_child.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/inc/com/centreon/common/file.hh"

using namespace com::centreon;
using namespace com::centreon::connector::perl;

// Perl interpreter object.
static PerlInterpreter* my_perl(nullptr);
// Allow module loading.
EXTERN_C void xs_init(pTHX);

constexpr std::string_view _LOADER_SCRIPT = R"(
#!/usr/bin/perl

package Embed::Persistent;

use Text::ParseWords qw(parse_line);

use constant MTIME_IDX  => 0;
use constant HANDLE_IDX => 1;

$| = 1;

sub valid_package_name {
  my ($string) = @_;
  # First pass.
  $string =~ s/([^A-Za-z0-9\/])/sprintf("_%2x", unpack("C", $1))/eg;
  # Second pass only for words starting with a digit.
  $string =~ s|/(\d)|sprintf("/_%2x", unpack("C", $1))|eg;
  # Dress it up as a real package name.
  $string =~ s|/|::|g;
  return "Embed" . $string;
}

sub eval_file {
  my ($filename) = @_;
  my $mtime = -M $filename;

  # Read Perl script.
  my $package = valid_package_name($filename);
  open(my $fh, "<", $filename)
    or die "failed to open Perl file '$filename': $!";
  my $sub;
  sysread $fh, $sub, -s $fh;
  close $fh;
  $sub =~ s/__END__/\;}
__END__/;

  # Wrap the code into a subroutine.
  my $hndlr = <<EOSUB;
package $package;


BEGIN {
    *CORE::GLOBAL::exit = sub {
        print "\\n\\nEXIT:", \$_[0], "\\n";
        die "EXIT \$_[0]\n";
    };
}

sub subroutine {
  \@ARGV = \@_;
  local \$^W = 1;
  $sub
}
EOSUB

 # Ensure modified Perl plugins get recached properly.
  no strict 'refs';
  undef %{$package.'::'};
  use strict 'refs';

  # Compile.
  eval $hndlr;
  if ($@) {
    chomp($@);
    die "syntax error in '$filename': $@";
  }

  no strict 'refs';
  return *{ $package . '::subroutine' }{CODE} ;
}

sub run_file {
  # Fetch arguments.
  my ($filename, $handle, $args) = @_;

  # Parse arguments.
  my @parsed_args = ("$filename");
  push(@parsed_args, parse_line('\s+', 0, $args));

  # Run subroutine.
  print "Run subroutine";
  my $res;
  eval { $res = $handle->(@parsed_args) };
  # if ($@) {
  #   chomp($@);
  #   die "could not run '$filename': $@";
  # }
  return ($res);
}

)";

constexpr std::string_view _SCRIPT_PATH = "/tmp/centreon_connector_perl.XXXXXX";

script_child::script_child(const std::shared_ptr<asio::io_context> io_context,
                           const std::shared_ptr<spdlog::logger>& logger,
                           const std::string& script_path,
                           const std::string& additional_code)
    : com::centreon::common::fork<false>(io_context, logger),
      _script_path(script_path),
      _additional_code(additional_code),
      _minute_timer(*_io_context) {}

script_child::~script_child() {
  SPDLOG_LOGGER_INFO(_logger, "cleaning up Embedded Perl");
  if (my_perl) {
    PL_perl_destruct_level = 1;
    perl_destruct(my_perl);
    perl_free(my_perl);
    PERL_SYS_TERM();
  }
}

std::string script_child::_write_loader_to_disk(
    const std::string_view& additional_code) {
  char script_path[_SCRIPT_PATH.length() + 1];
  strcpy(script_path, _SCRIPT_PATH.data());
  int script_fd = mkstemp(script_path);
  if (script_fd < 0) {
    char const* msg(strerror(errno));
    throw exceptions::msg_fmt(
        "could not create temporary file for loader {}: {}", script_path, msg);
  }

  std::list<std::string_view> contents;
  contents.push_back(_LOADER_SCRIPT);
  if (!additional_code.empty()) {
    contents.push_back(additional_code);
    contents.push_back("\n");
  }

  for (const std::string_view content : contents) {
    size_t len = content.length();
    size_t offset = 0;
    while (len > 0) {
      ssize_t wb(write(script_fd, content.data() + offset, len));
      if (wb <= 0) {
        char const* msg(strerror(errno));
        close(script_fd);
        unlink(script_path);
        throw exceptions::msg_fmt("could not write embedded script {}: {}",
                                  script_path, msg);
      }
      len -= wb;
      offset += wb;
    }
  }

  return script_path;
}

void script_child::_compile_script(const std::string& loader_path) {
  SPDLOG_LOGGER_INFO(_logger, "loading Embedded Perl interpreter");

  if (!(my_perl = perl_alloc())) {
    SPDLOG_LOGGER_ERROR(_logger, "could not allocate Perl interpreter");
  }
  perl_construct(my_perl);
  PL_origalen = 1;
  PL_perl_destruct_level = 1;

  // Parse embedded script.
  const char* embedding[2];
  embedding[0] = "";
  embedding[1] = loader_path.c_str();
  if (perl_parse(my_perl, &xs_init, sizeof(embedding) / sizeof(*embedding),
                 (char**)embedding, nullptr)) {
    SPDLOG_LOGGER_ERROR(_logger, "could not parse embedded Perl script");
    throw exceptions::msg_fmt("could not parse embedded Perl script");
  }
  PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
  perl_run(my_perl);
}

void script_child::_load_check_script() {
  // load check script
  std::filesystem::file_time_type check_script_mtime;
  std::string file_content;
  try {
    check_script_mtime = std::filesystem::last_write_time(_script_path);
    if (_check_script_handle &&
        check_script_mtime ==
            _check_script_mtime) {  // script yet loaded and not updated =>
                                    // nothing to do
      return;
    }
    file_content = com::centreon::common::read_file_content(_script_path);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "could not load check script {}",
                        _script_path);
    throw exceptions::msg_fmt("could not load check script {}", _script_path);
  }

  // Compile Perl file.
  dSP;
  {
    SPDLOG_LOGGER_DEBUG(_logger, "parsing file {}", file);
    const char* argv[3];
    argv[0] = file_content.c_str();
    argv[1] = "0";
    argv[2] = nullptr;
    if (call_argv("Embed::Persistent::eval_file", G_EVAL | G_SCALAR,
                  (char**)argv) != 1)
      throw exceptions::msg_fmt("could not compile Perl script {}",
                                _script_path);
  }
  SPAGAIN;
  SV* handle = POPs;
  if (SvTRUE(ERRSV)) {
    throw exceptions::msg_fmt("Embedded Perl error: {}", SvPV_nolen(ERRSV));
  }
  _check_script_mtime = check_script_mtime;
  _check_script_handle = handle;
}

int script_child::_run(int stdin_fd, int stdout_fd, int stderr_fd) {
  _child_stdout =
      std::make_unique<asio::writable_pipe>(*_io_context, stdout_fd);
  std::string loader_path;
  try {
    loader_path = _write_loader_to_disk(_additional_code);
  } catch (const std::exception& e) {
    _global_error = e.what();
  }
  if (_global_error.empty()) {
    try {
      _compile_script(loader_path);
    } catch (const std::exception& e) {
      _global_error = e.what();
    }
  }
  if (_global_error.empty()) {
    try {
      _load_check_script();
    } catch (const std::exception& e) {
      _global_error = e.what();
    }
  }
  if (!_global_error.empty()) {
    ConnectorMess error;
    error.mutable_have_to_terminate()->set_error(_global_error);
    _protocol.send(*_child_stdout, error);
    return -1;
  }
  _io_context->run();
  return 0;
}

void script_child::write_mess_to_child_stdin(const ConnectorMess& mess) {
  auto buff = _protocol.serialize(mess);
  write_to_child_stdin(
      std::string(reinterpret_cast<const char*>(buff.get()), buff->len));
}

void script_child::_start_minute_timer() {
  _minute_timer.expires_after(std::chrono::minutes(1));
  _minute_timer.async_wait(
      [this, me = shared_from_this()](const boost::system::error_code& err) {
        if (!err) {
          _minute_timer_handler();
        }
      });
}

void script_child::_minute_timer_handler() {
  std::error_code err;
  auto check_script_mtime = std::filesystem::last_write_time(_script_path, err);
  if (!err &&
      check_script_mtime != _check_script_mtime) {  // script updated => reload
    ConnectorMess error;
    error.mutable_have_to_terminate()->set_error(
        fmt::format("{} updated, need to reload", _script_path));
    _protocol.async_send(*_child_stdout, error,
                         [](const boost::system::error_code&) {});
  }

  _start_minute_timer();
}
