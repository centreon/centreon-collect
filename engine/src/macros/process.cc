/**
 * Copyright 1999-2010 Ethan Galstad
 * Copyright 2011-2013 Merethis
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "com/centreon/engine/macros/process.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/string.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;

/*
 * replace macros in notification commands with their values,
 * the thread-safe version
 */
int process_macros_r(nagios_macros* mac,
                     const std::string_view& input_buffer,
                     std::string& output_buffer,
                     int options) {
  std::string cleaned_macro;
  int result = OK;
  int clean_options = 0;
  int free_macro = false;
  int macro_options = 0;

  SPDLOG_LOGGER_TRACE(functions_logger, "process_macros_r()");

  if (input_buffer.empty()) {
    return ERROR;
  }

  output_buffer.reserve(input_buffer.length());
  output_buffer.clear();

  SPDLOG_LOGGER_TRACE(macros_logger,
                      "**** BEGIN MACRO PROCESSING **** Processing: '{}'",
                      input_buffer);

  // clang-format off
  /*we have two kinds of tags for macros
  $_SERVICEMAC$: resolved as a service _MAC macro. 
        If it exists, it is replaced by its content
        If not, if we are at first level of recursion, $_SERVICEMAC$ is replaced by an empty string
                if we are evaluating content of a macro, $_SERVICEMAC$ stays in place
  {{$_SERVICEMAC$}}: resolved as a service _MAC macro. 
        If it exists, it is replaced by its content
        If not, {{$_SERVICEMAC$}} is replaced by an empty string IN ALL CASES
  */
  // clang-format on
  size_t offset = 0;
  while (offset < input_buffer.length()) {
    size_t tag_begin = input_buffer.find('$', offset);
    if (tag_begin == std::string_view::npos) {
      break;
    }

    enum { simple_dollar_tag, brace_dollar_tag } tag_type;
    if ((tag_begin + 1) < input_buffer.length() &&
        input_buffer[tag_begin + 1] == '$') {  //$$ escape}
      output_buffer.append(input_buffer, offset, tag_begin - offset + 1);
      offset = tag_begin + 2;
      continue;
    } else if (tag_begin >= 2 && input_buffer[tag_begin - 1] == '{' &&
               input_buffer[tag_begin - 2] == '{') {
      output_buffer.append(input_buffer, offset, tag_begin - offset - 2);
      offset = tag_begin - 2;
      tag_type = brace_dollar_tag;
    } else {
      output_buffer.append(input_buffer, offset, tag_begin - offset);
      offset = tag_begin;
      tag_type = simple_dollar_tag;
    }
    // search end of macro name
    size_t tag_end = tag_type == simple_dollar_tag
                         ? input_buffer.find('$', tag_begin + 1)
                         : input_buffer.find("$}}", tag_begin + 1);
    // no end => we keep all string
    if (tag_end == std::string_view::npos) {
      break;
    }
    std::string_view token =
        input_buffer.substr(tag_begin + 1, tag_end - tag_begin - 1);

    std::string token_resolved;
    /* reset clean options */
    clean_options = 0;

    /* grab the macro value */
    result = grab_macro_value_r(mac, token, token_resolved, &clean_options,
                                &free_macro);

    SPDLOG_LOGGER_TRACE(
        macros_logger, "  Processed '{}', To '{}', Clean Options: {}, Free: {}",
        token, token_resolved, clean_options, free_macro);

    if (result == OK) {
      /* include any cleaning options passed back to us */
      macro_options = (options | clean_options);

      SPDLOG_LOGGER_TRACE(
          macros_logger,
          "  Cleaning options: global={}, local={}, effective={}", options,
          clean_options, macro_options);

      /* some macros are cleaned... */
      if ((macro_options & STRIP_ILLEGAL_MACRO_CHARS) ||
          (macro_options & ESCAPE_MACRO_CHARS)) {
        /* add the (cleaned) processed macro to the end of the already
         * processed buffer */
        if (!token_resolved.empty()) {
          cleaned_macro = clean_macro_chars(token_resolved, macro_options);
          if (!cleaned_macro.empty()) {
            output_buffer.append(cleaned_macro);

            SPDLOG_LOGGER_TRACE(macros_logger,
                                "  Cleaned macro.  Running output ({}): '{}'",
                                output_buffer.length(), output_buffer);
          }
        }
      } else {
        /* add the processed macro to the end of the already processed
         * buffer */
        output_buffer.append(token_resolved);

        SPDLOG_LOGGER_TRACE(macros_logger,
                            "  Uncleaned macro.  Running output ({}): '{}'",
                            output_buffer.length(), output_buffer);
      }

      SPDLOG_LOGGER_TRACE(macros_logger,
                          "  Just finished macro.  Running output ({}): '{}'",
                          output_buffer.length(), output_buffer);
    } else {
      /* an error occurred - we couldn't parse the macro, so continue on */
      SPDLOG_LOGGER_TRACE(macros_logger,
                          " WARNING: An error occurred processing macro '{}'!",
                          token);

      // for reason of backward compatibility simple dollar macros name are not
      // removed while decoding content of macros
      // example: _FILTER         name in ('MSSQL$SIG','MSSQL$RH')
      if (tag_type == simple_dollar_tag &&
          mac->custom_macro_recursion_depth > 0) {
        output_buffer +=
            input_buffer.substr(tag_begin, tag_end - tag_begin + 1);
      }
    }

    offset = tag_type == simple_dollar_tag ? tag_end + 1 : tag_end + 3;
  }

  // append any trailing text after the last macro
  if (offset < input_buffer.length()) {
    output_buffer.append(input_buffer, offset);
  }
  SPDLOG_LOGGER_TRACE(
      macros_logger,
      "  Done.  Final output: '{}' **** END MACRO PROCESSING ****",
      output_buffer);
  return OK;
}
