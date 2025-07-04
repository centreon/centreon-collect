/**
 * Copyright 2024 Centreon
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

#include "file.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

namespace com::centreon::common {

/**
 * @brief Reads the content of a text file and returns it in an std::string.
 *
 * @param file_path The file to read.
 *
 * @return The content as an std::string.
 */
std::string read_file_content(const std::filesystem::path& file_path) {
  // Is path a readable file?
  if (!std::filesystem::is_regular_file(file_path)) {
    throw exceptions::msg_fmt("File '{}' is not a regular file",
                              file_path.string());
  }
  std::ifstream in(file_path, std::ios::in);
  std::string retval;
  if (in) {
    in.seekg(0, std::ios::end);
    retval.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&retval[0], retval.size());
    in.close();
  } else
    throw exceptions::msg_fmt("Can't open file '{}': {}", file_path.string(),
                              strerror(errno));
  return retval;
}

/**
 * @brief Compute the hash of a directory content.
 *
 * @param dir_path The directory to parse.
 *
 * @return a size_t hash.
 */
std::string hash_directory(const std::filesystem::path& dir_path,
                           std::error_code& ec) noexcept {
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
}  // namespace com::centreon::common
