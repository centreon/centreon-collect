/**
 * Copyright 2024,2026 Centreon
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
#include <dirent.h>

#include "absl/strings/numbers.h"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "file_system.hh"

namespace com::centreon::common {

/**
 * @brief Reads the content of a text file and returns it in an std::string.
 * optim: two versions
 *
 * @param file_path The file to read.
 *
 * @return The content as an std::string.
 */

#ifndef _WIN32
std::string read_file_content(const std::filesystem::path& file_path) {
  int fd = open(file_path.c_str(), O_RDONLY);
  if (fd < 0)
    throw exceptions::msg_fmt("Can't open file '{}': {}", file_path.string(),
                              strerror(errno));

  struct stat st;
  if (fstat(fd, &st) != 0) {
    close(fd);
    throw exceptions::msg_fmt("Can't get file size '{}': {}",
                              file_path.string(), strerror(errno));
  }

  size_t fileSize = static_cast<size_t>(st.st_size);

  std::string result;
  if (fileSize > 0 && fileSize < SIZE_MAX) {  // if special files
    result.reserve(fileSize);
  }

  char buffer[64 * 1024];  // 64 KB

  while (true) {
    ssize_t n = read(fd, buffer, sizeof(buffer));
    if (n <= 0)
      break;

    result.append(buffer, static_cast<size_t>(n));
  }

  close(fd);
  return result;
}

#else

std::string read_file_content(const std::filesystem::path& file_path) {
  std::ifstream in(file_path, std::ios::in);
  std::string retval;
  if (in) {
    in.seekg(0, std::ios::end);
    auto size = in.tellg();
    if (size == -1) {
      throw exceptions::msg_fmt("Can't get file size '{}': {}",
                                file_path.string(), strerror(errno));
    }
    retval.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&retval[0], retval.size());
    in.close();
  } else
    throw exceptions::msg_fmt("Can't open file '{}': {}", file_path.string(),
                              strerror(errno));
  return retval;
}
#endif

/**
 * @brief Compute the hash of a directory content.
 *
 * @param dir_path The directory to parse.
 *
 * @return a size_t hash.
 */
std::string hash_directory(const std::filesystem::path& dir_path,
                           std::error_code& ec) {
  std::list<std::filesystem::path> files;
  ec.clear();

  /* Recursively parse the directory */
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(dir_path, ec)) {
    if (entry.is_regular_file() && entry.path().extension() == ".cfg")
      files.push_back(entry.path());
  }

  if (ec)
    return "";

  files.sort();

  EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr);

  for (auto& f : files) {
    const std::string& fname =
        std::filesystem::relative(f, dir_path, ec).string();
    if (ec)
      break;
    EVP_DigestUpdate(mdctx, fname.data(), fname.size());
    std::string content = read_file_content(f);
    EVP_DigestUpdate(mdctx, content.data(), content.size());
  }

  unsigned char hash[SHA256_DIGEST_LENGTH];
  unsigned int size;
  EVP_DigestFinal_ex(mdctx, hash, &size);
  EVP_MD_CTX_free(mdctx);

  if (ec)
    return "";

  std::string retval;
  retval.reserve(SHA256_DIGEST_LENGTH * 2);
  auto digit = [](unsigned char d) -> char {
    if (d < 10)
      return '0' + d;
    else
      return 'a' + (d - 10);
  };

  for (auto h : hash) {
    retval.push_back(digit(h >> 4));
    retval.push_back(digit(h & 0xf));
  }
  return retval;
}

static void _dir_content_impl(const std::filesystem::path& dir_path,
                              bool recursive,
                              bool remove_directory_fd_from_result,
                              std::list<std::filesystem::path>& result) {
  constexpr size_t buf_size = 65536;
  char buf[buf_size];

  int fd = open(dir_path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0)
    throw exceptions::msg_fmt("Can't open directory '{}': {}",
                              dir_path.string(), strerror(errno));

  for (;;) {
    long nread = syscall(SYS_getdents64, fd, buf, buf_size);
    if (nread < 0) {
      close(fd);
      throw exceptions::msg_fmt("getdents64 failed on '{}': {}",
                                dir_path.string(), strerror(errno));
    }
    if (nread == 0)
      break;

    for (long pos = 0; pos < nread;) {
      auto* entry = reinterpret_cast<struct dirent64*>(buf + pos);
      pos += entry->d_reclen;

      std::string_view name(entry->d_name);

      if (remove_directory_fd_from_result) {
        int numeric_name;
        if (absl::SimpleAtoi(name, &numeric_name) && numeric_name == fd)
          continue;
      }
      if (name == "." || name == "..")
        continue;

      std::filesystem::path entry_path = dir_path / name;

      if (entry->d_type == DT_DIR) {
        if (recursive)
          _dir_content_impl(entry_path, true, remove_directory_fd_from_result,
                            result);
      } else {
        result.push_back(std::move(entry_path));
      }
    }
  }

  close(fd);
}
/**
 *  Fill a path list with the files listed in the directory.
 *
 * @param path The directory path
 * @param recursive if true, we iterate in sub directories
 * @param remove_directory_fd_from_result if true, when we walk in
 * /proc/self/fd, we don't report opened directory from result
 *
 * @return a list of names.
 */
std::list<std::filesystem::path> dir_content(
    const std::filesystem::path& dir_path,
    bool recursive,
    bool remove_directory_fd_from_result) {
  std::list<std::filesystem::path> result;
  _dir_content_impl(dir_path, recursive, remove_directory_fd_from_result,
                    result);
  return result;
}

}  // namespace com::centreon::common
