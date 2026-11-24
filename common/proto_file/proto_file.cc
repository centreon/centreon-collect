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
#include "common/proto_file/proto_file.hh"

#include <zstd.h>

#include <atomic>
#include <cerrno>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>

namespace com::centreon::common {

namespace {
/* Header of a compressed file: a magic, the format version and the compression
 * algorithm, then the compressed message. The magic starts with a null byte,
 * which can never start a serialized protobuf message (field number 0 does not
 * exist): a file without it is a plain serialized message, as written by the
 * previous versions or without compression.
 *
 * "CBC" stands for Centreon Broker Cache, the first file compressed this way:
 * the magic is kept so that the cache files already written stay readable, but
 * it is not specific to the cache any more. */
constexpr std::string_view magic{"\0CBC", 4};
constexpr char format_version = 1;
constexpr char algorithm_zstd = 1;
constexpr size_t header_size = magic.size() + 2;
/* Level 1: on the Broker cache files, it compresses better than zlib at its
 * highest level, eight times faster than zlib at its default level. */
constexpr int zstd_level = 1;

class format_error_category : public std::error_category {
 public:
  const char* name() const noexcept override { return "proto_file"; }
  std::string message(int ev) const override {
    switch (static_cast<proto_file_errc>(ev)) {
      case proto_file_errc::truncated_header:
        return "truncated compression header";
      case proto_file_errc::unknown_format_version:
        return "unknown compression format version";
      case proto_file_errc::unknown_algorithm:
        return "unknown compression algorithm";
      case proto_file_errc::corrupted_content:
        return "corrupted compressed content";
      case proto_file_errc::compression_error:
        return "compression failed";
      case proto_file_errc::parse_error:
        return "cannot parse the protobuf message";
      case proto_file_errc::serialize_error:
        return "cannot serialize the protobuf message";
    }
    return "unknown proto_file error";
  }
};

/**
 * @brief The current errno as an error code, or a plain I/O error when the
 * stream that failed did not set it.
 */
std::error_code last_system_error() noexcept {
  const int err = errno;
  return err ? std::error_code(err, std::generic_category())
             : std::make_error_code(std::errc::io_error);
}

/**
 * @brief Compress a serialized message and prepend the header.
 *
 * @param serialized The serialized message.
 * @param ec Set on failure.
 *
 * @return The content of the file, empty on failure.
 */
std::string compress_content(std::string_view serialized, std::error_code& ec) {
  std::string retval(header_size + ZSTD_compressBound(serialized.size()), '\0');
  retval.replace(0, magic.size(), magic);
  retval[magic.size()] = format_version;
  retval[magic.size() + 1] = algorithm_zstd;

  std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> cctx{ZSTD_createCCtx(),
                                                            ZSTD_freeCCtx};
  if (!cctx) {
    ec = std::make_error_code(std::errc::not_enough_memory);
    return {};
  }
  ZSTD_CCtx_setParameter(cctx.get(), ZSTD_c_compressionLevel, zstd_level);
  /* The checksum makes a corrupted file detected instead of parsed. */
  ZSTD_CCtx_setParameter(cctx.get(), ZSTD_c_checksumFlag, 1);
  const size_t size = ZSTD_compress2(cctx.get(), retval.data() + header_size,
                                     retval.size() - header_size,
                                     serialized.data(), serialized.size());
  if (ZSTD_isError(size)) {
    ec = proto_file_errc::compression_error;
    return {};
  }
  retval.resize(header_size + size);
  return retval;
}

/**
 * @brief Decompress what follows the magic of a compressed file.
 *
 * @param content The content of the file after the magic: format version,
 * algorithm, then the compressed message.
 * @param ec Set on failure.
 *
 * @return The serialized message, empty on failure.
 */
std::string uncompress_content(std::string_view content, std::error_code& ec) {
  if (content.size() < header_size - magic.size()) {
    ec = proto_file_errc::truncated_header;
    return {};
  }
  if (content[0] != format_version) {
    ec = proto_file_errc::unknown_format_version;
    return {};
  }
  if (content[1] != algorithm_zstd) {
    ec = proto_file_errc::unknown_algorithm;
    return {};
  }
  content.remove_prefix(2);
  const unsigned long long size =
      ZSTD_getFrameContentSize(content.data(), content.size());
  if (size == ZSTD_CONTENTSIZE_ERROR || size == ZSTD_CONTENTSIZE_UNKNOWN) {
    ec = proto_file_errc::corrupted_content;
    return {};
  }
  std::string retval(size, '\0');
  const size_t res = ZSTD_decompress(retval.data(), retval.size(),
                                     content.data(), content.size());
  if (ZSTD_isError(res) || res != size) {
    ec = proto_file_errc::corrupted_content;
    return {};
  }
  return retval;
}
}  // namespace

const std::error_category& proto_file_error_category() noexcept {
  static const format_error_category instance;
  return instance;
}

std::error_code make_error_code(proto_file_errc e) noexcept {
  return {static_cast<int>(e), proto_file_error_category()};
}

/**
 * @brief Write a protobuf message to a file, whole or not at all.
 *
 * The message goes to a temporary file next to @p path, renamed over it once
 * complete: a reader never sees a partial file, and a writer killed on the way
 * leaves the previous one intact. The temporary name does not end like
 * @p path, so no scan of the directory takes it for a file of the same kind.
 * It is unique, as two writers of the same file are not always serialized by
 * the same lock.
 *
 * The file is not synced before the rename: it survives the death of the
 * process, not a power loss.
 *
 * @param path The file to write.
 * @param msg The message to store.
 * @param compress Whether to compress the message with zstd, behind a header
 * that read_proto_file() recognizes.
 * @param ec Cleared on success, set to the reason of the failure otherwise; the
 * file is then left as it was.
 */
void write_proto_file(const std::filesystem::path& path,
                      const google::protobuf::Message& msg,
                      bool compress,
                      std::error_code& ec) noexcept {
  static std::atomic<uint64_t> counter{0};
  ec.clear();
  try {
    std::filesystem::path tmp(path);
    tmp += ".tmp" + std::to_string(counter++);
    {
      std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
      if (!f) {
        ec = last_system_error();
        return;
      }
      if (compress) {
        std::string serialized;
        if (!msg.SerializeToString(&serialized))
          ec = proto_file_errc::serialize_error;
        else {
          const std::string content = compress_content(serialized, ec);
          if (!ec && !f.write(content.data(), content.size()))
            ec = last_system_error();
        }
      } else if (!msg.SerializeToOstream(&f)) {
        /* A failed stream says the disk refused; a sound stream says the
         * message itself could not be serialized. */
        ec = f ? make_error_code(proto_file_errc::serialize_error)
               : last_system_error();
      }
      f.close();
      if (!ec && !f)
        ec = last_system_error();
    }
    if (!ec)
      std::filesystem::rename(tmp, path, ec);
    if (ec) {
      std::error_code ignored;
      std::filesystem::remove(tmp, ignored);
    }
  } catch (const std::bad_alloc&) {
    ec = std::make_error_code(std::errc::not_enough_memory);
  }
}

/**
 * @brief Read a protobuf message from a file, compressed or not: the header of
 * the file tells.
 *
 * A file without the header is parsed straight from the stream, as it always
 * was, without being copied in memory first. A compressed one is read whole,
 * then decompressed.
 *
 * @param path The file to read.
 * @param msg The message to fill; in an unspecified state on failure.
 * @param ec Cleared on success, set to the reason of the failure otherwise.
 */
void read_proto_file(const std::filesystem::path& path,
                     google::protobuf::Message& msg,
                     std::error_code& ec) noexcept {
  ec.clear();
  try {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
      ec = last_system_error();
      return;
    }
    char head[magic.size()];
    f.read(head, sizeof(head));
    if (f.gcount() == static_cast<std::streamsize>(sizeof(head)) &&
        std::string_view(head, sizeof(head)) == magic) {
      const std::string content{std::istreambuf_iterator<char>{f},
                                std::istreambuf_iterator<char>{}};
      if (f.bad()) {
        ec = last_system_error();
        return;
      }
      const std::string serialized = uncompress_content(content, ec);
      if (!ec && !msg.ParseFromString(serialized))
        ec = proto_file_errc::parse_error;
      return;
    }
    f.clear();
    f.seekg(0);
    if (!msg.ParseFromIstream(&f))
      ec = f.bad() ? last_system_error()
                   : make_error_code(proto_file_errc::parse_error);
  } catch (const std::bad_alloc&) {
    ec = std::make_error_code(std::errc::not_enough_memory);
  }
}

}  // namespace com::centreon::common
