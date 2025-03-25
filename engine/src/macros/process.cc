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
                     std::string const& input_buffer,
                     std::string& output_buffer,
                     int options) {
  std::string selected_macro;
  std::string cleaned_macro;
  int clean_macro = false;
  int result = OK;
  int clean_options = 0;
  int free_macro = false;
  int macro_options = 0;

  engine_logger(dbg_functions, basic) << "process_macros_r()";
  functions_logger->trace("process_macros_r()");

  output_buffer = "";

  if (input_buffer.empty())
    return ERROR;

  engine_logger(dbg_macros, more) << "**** BEGIN MACRO PROCESSING ***********\n"
                                     "Processing: '"
                                  << input_buffer << "'";
  macros_logger->trace("**** BEGIN MACRO PROCESSING **** Processing: '{}'",
                       input_buffer);

  for (std::string::const_iterator it{input_buffer.begin()},
       end{input_buffer.end()};
       it != end; ++it) {
    if (*it == '$') {
      if (std::next(it) == input_buffer.end())
        ;                                // last character is a dollar
      else if (*std::next(it) == '$') {  //$$ => $ escape
        output_buffer += "$";
        ++it;
      } else {
        long int where{it - input_buffer.begin()};
        size_t pos{input_buffer.find("$", where + 1)};

        if (pos != std::string::npos) {
          std::string_view token(input_buffer.data() + where + 1,
                                 pos - where - 1);
          // std::string const& token(
          //     input_buffer.substr(where + 1, pos - where - 1));
          std::string token_resolved;
          /* reset clean options */
          clean_options = 0;

          /* grab the macro value */
          result = grab_macro_value_r(mac, token, token_resolved,
                                      &clean_options, &free_macro);

          engine_logger(dbg_macros, most)
              << "  Processed '" << token << "', To '" << token_resolved
              << "', Clean Options: " << clean_options
              << ", Free: " << free_macro;
          macros_logger->trace(
              "  Processed '{}', To '{}', Clean Options: {}, Free: {}", token,
              token_resolved, clean_options, free_macro);

          /* an error occurred - we couldn't parse the macro, so continue on */
          if (result == ERROR) {
            engine_logger(dbg_macros, basic)
                << " WARNING: An error occurred processing macro '" << token
                << "'!";
            macros_logger->trace(
                " WARNING: An error occurred processing macro '{}'!", token);
          }

          /* insert macro */
          if (!token_resolved.empty()) {
            engine_logger(dbg_macros, most)
                << "  Processed '" << token
                << "', Clean Options: " << clean_options
                << ", Free: " << free_macro;
            macros_logger->trace(
                "  Processed '{}', Clean Options: {}, Free: {}", token,
                clean_options, free_macro);

            /* include any cleaning options passed back to us */
            macro_options = (options | clean_options);

            engine_logger(dbg_macros, most)
                << "  Cleaning options: global=" << options
                << ", local=" << clean_options
                << ", effective=" << macro_options;
            macros_logger->trace(
                "  Cleaning options: global={}, local={}, effective={}",
                options, clean_options, macro_options);

            /* some macros are cleaned... */
            if (clean_macro || (macro_options & STRIP_ILLEGAL_MACRO_CHARS) ||
                (macro_options & ESCAPE_MACRO_CHARS)) {
              /* add the (cleaned) processed macro to the end of the already
               * processed buffer */
              if (!token_resolved.empty()) {
                cleaned_macro =
                    clean_macro_chars(token_resolved, macro_options);
                if (!cleaned_macro.empty()) {
                  output_buffer.append(cleaned_macro);

                  engine_logger(dbg_macros, basic)
                      << "  Cleaned macro.  Running output ("
                      << output_buffer.length() << "): '" << output_buffer
                      << "'";
                  macros_logger->trace(
                      "  Cleaned macro.  Running output ({}): '{}'",
                      output_buffer.length(), output_buffer);
                }
              }
            } else {
              /* add the processed macro to the end of the already processed
               * buffer */
              output_buffer.append(token_resolved);

              engine_logger(dbg_macros, basic)
                  << "  Uncleaned macro.  Running output ("
                  << output_buffer.length() << "): '" << output_buffer << "'";
              macros_logger->trace(
                  "  Uncleaned macro.  Running output ({}): '{}'",
                  output_buffer.length(), output_buffer);
            }

            /* free memory if necessary (if we URL encoded the macro or we were
             * told to do so by grab_macro_value()) */
            engine_logger(dbg_macros, basic)
                << "  Just finished macro.  Running output ("
                << output_buffer.length() << "): '" << output_buffer << "'";
            macros_logger->trace(
                "  Just finished macro.  Running output ({}): '{}'",
                output_buffer.length(), output_buffer);
          }

          it += pos - where;
        }
      }
    } else {
      output_buffer += *it;
    }
  }

  engine_logger(dbg_macros, more) << "  Done.  Final output: '" << output_buffer
                                  << "'\n"
                                     "**** END MACRO PROCESSING *************";
  macros_logger->trace(
      "  Done.  Final output: '{}' **** END MACRO PROCESSING ****",
      output_buffer);
  return OK;
}
