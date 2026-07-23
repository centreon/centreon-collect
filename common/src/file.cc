/**
 * Copyright 2024-2026 Centreon
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

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include "com/centreon/exceptions/msg_fmt.hh"
#include "google/protobuf/message.h"

#include "file.hh"

namespace com::centreon::common {

/**
 * @brief Reads the content of a text file and returns it in an std::string.
 *
 * @param file_path The file to read.
 *
 * @return The content as an std::string.
 */
std::string read_file_content(const std::filesystem::path& file_path) {
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

/**
 * @brief Maps file_path in memory and parses its content into data. The file
 * is read without any copy in an intermediate buffer.
 *
 * @param file_path The file to read.
 * @param data The message to fill.
 *
 * @return false if the file doesn't exist or isn't readable (data is left
 * untouched), true if data has been filled.
 *
 * @throw boost::interprocess::interprocess_exception if the file can't be
 * mapped.
 * @throw exceptions::msg_fmt if the content is not a valid serialization of
 * data.
 */
bool load_proto_from_disk(const std::filesystem::path& file_path,
                          ::google::protobuf::Message& data) {
  if (::access(file_path.c_str(), R_OK)) {
    return false;
  }
  std::error_code ec;
  if (std::filesystem::file_size(file_path, ec) == 0 && !ec) {
    data.Clear();
    return true;
  }
  boost::interprocess::file_mapping file_map(file_path.c_str(),
                                             boost::interprocess::read_only);
  boost::interprocess::mapped_region region(file_map,
                                            boost::interprocess::read_only);
  if (!data.ParseFromArray(region.get_address(), region.get_size())) {
    throw exceptions::msg_fmt("Fail to parse {}", file_path.string());
  }
  return true;
}

/**
 * @brief Serializes data in a temporary file created in the same directory as
 * file_path, then renames it to file_path. So readers never see a partially
 * written file.
 *
 * @param file_path The destination file.
 * @param data The message to serialize.
 *
 * @throw exceptions::msg_fmt if the temporary file can't be created, resized
 * or mapped, or if it can't be renamed to file_path. In all these cases, the
 * temporary file is removed.
 */
void save_proto_to_disk(const std::filesystem::path& file_path,
                        const ::google::protobuf::Message& data) {
  size_t needed = data.ByteSizeLong();

  // mkstemp replaces XXXXXX and needs a null-terminated mutable buffer
  std::string tmp_path;
  tmp_path.reserve(file_path.string().length() + 7);
  tmp_path.append(file_path);
  tmp_path.append(".XXXXXX");

  int fd = ::mkstemp(tmp_path.data());
  if (fd < 0) {
    throw exceptions::msg_fmt("Fail to create temporary file from {}: {}",
                              tmp_path, strerror(errno));
  }

  if (needed > 0) {
    // posix_fallocate refuses a zero length
    if (int err = ::posix_fallocate(fd, 0, needed); err) {
      ::close(fd);
      ::unlink(tmp_path.c_str());
      throw exceptions::msg_fmt("Fail to resize {} to {} bytes: {}", tmp_path,
                                needed, strerror(err));
    }
    // mmap refuses a zero length, an empty message needs no mapping
    void* addr =
        ::mmap(nullptr, needed, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
      int err = errno;
      ::close(fd);
      ::unlink(tmp_path.c_str());
      throw exceptions::msg_fmt("Fail to map {}: {}", tmp_path, strerror(err));
    }
    data.SerializeWithCachedSizesToArray(static_cast<uint8_t*>(addr));
    ::munmap(addr, needed);
  }

  ::close(fd);

  if (::rename(tmp_path.c_str(), file_path.c_str()) < 0) {
    int err = errno;
    ::unlink(tmp_path.c_str());
    throw exceptions::msg_fmt("Fail to rename {} to {}: {}", tmp_path,
                              file_path.string(), strerror(err));
  }
}

}  // namespace com::centreon::common
