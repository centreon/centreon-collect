/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <unistd.h>

/**
 *  Find line into the environement.
 *
 *  @param[in] data line on form key=value.
 *  @param[in] env  The environment.
 *
 *  @return True if data is find into env, otherwise false.
 */
static bool find(char const* data, char** env) {
  for (unsigned int i(0); env[i]; ++i)
    if (!strcmp(data, env[i]))
      return (true);
  return (false);
}

/**
 *  Check if the content of ref is in env array.
 *
 *  @param[in] ref The content reference.
 *  @param[in] env The content to check.
 *
 *  @return EXIT_SUCCESS on success, otherwise EXIT_FAILURE.
 */
static int check_env(char** ref, char** env) {
  for (unsigned int i(0); ref[i]; ++i)
    if (!find(ref[i], env))
      return (EXIT_FAILURE);
  return (EXIT_SUCCESS);
}

/**
 *  Read stdin and write into the stdout.
 *
 *  @param[in] type If type if "err" write data on stderr,
 *                  otherwise write on stdout.
 *
 *  @return The total bytes written.
 */
static int check_output(char const* type) {
  int output(strcmp(type, "err") ? 1 : 2);
  int total(0);
  int size(0);
  char buffer[1024];
  while ((size = read(0, buffer, sizeof(buffer))) > 0)
    total += write(output, buffer, size);
  return (EXIT_SUCCESS);
}

/**
 *  Usage of the application.
 *
 *  @param[in] appname The application name.
 */
static void usage(char const* appname) {
  std::cerr
    << "usage: " << appname << std::endl
    << "  check_env key1=value1 keyx=valuex..." << std::endl
    << "  check_output err|out" << std::endl
    << "  check_return value" << std::endl
    << "  check_sleep value" << std::endl;
  exit(EXIT_FAILURE);
}

/**
 *  This main test the class process.
 */
int main(int argc, char** argv, char** env) {
  try {
    if (argc != 1) {
      if (!strcmp(argv[1], "check_env"))
        return (check_env(argv + 2, env));
      if (argc == 3) {
        if (!strcmp(argv[1], "check_output"))
          return (check_output(argv[2]));
        if (!strcmp(argv[1], "check_return"))
          return (atoi(argv[2]));
        if (!strcmp(argv[1], "check_sleep")) {
          int timeout(atoi(argv[2]));
          sleep(timeout);
          return (timeout);
        }
      }
    }
    usage(argv[0]);
  }
  catch (std::exception const& e) {
    std::cerr << "error:" << e.what() << std::endl;
    return (EXIT_FAILURE);
  }
}
