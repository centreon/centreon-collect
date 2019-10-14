/*
** Copyright 2011-2013 Centreon
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

#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif  // Win32 or POSIX

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
  std::cerr << "usage: " << appname << std::endl
            << "  check_env key1=value1 keyx=valuex..." << std::endl
            << "  check_output err|out" << std::endl << "  check_return value"
            << std::endl << "  check_sleep value" << std::endl;
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
#ifdef _WIN32
          Sleep(timeout * 1000);
#else
          sleep(timeout);
#endif  // Win32 or POSIX
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
