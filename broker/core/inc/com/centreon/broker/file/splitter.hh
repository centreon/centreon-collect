/**
 * Copyright 2017 Centreon
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

#ifndef CCB_FILE_SPLITTER_HH
#define CCB_FILE_SPLITTER_HH

#include "com/centreon/broker/file/fs_file.hh"
#include "common/log_v2/log_v2.hh"

using log_v2 = com::centreon::common::log_v2::log_v2;

namespace com::centreon::broker {

namespace file {
/**
 *  @class splitter splitter.hh "com/centreon/broker/file/splitter.hh"
 *  @brief Manage multi-file splitting.
 *
 *  Handle logical file splitting across multiple real files to
 *  provide easier file management.
 *
 *  How does it work.
 *  A splitter is made of several files. They all have the same base name but
 *  from the second one, this name is follow by a number which is incremented
 *  each time a new file is created.
 *
 *  We don't want to lock accesses to those files each time a reading or a
 *  writing is done. But we must lock accesses when:
 *  * we read and write on the same file.
 *  * we write and write on the same file.
 *
 *  To aquieve this, all write operations are protected by a single mutex
 *  _write_m. When a new sub-file is created, when we write to a file, this
 *  mutex is locked.
 *
 *  We can have several producers, that's why all write operations are
 *  protected. But we only have one consumer. Two possibilities, the reader
 *  reads a file already written and there is no need to have a protection.
 *  The current read file is also written, and it is mandatory to protect the
 *  access. So in the read method the _write_m mutex is locked only when we
 *  read in the same file as we write into.
 *
 *  _rid and _wid are indexes to the current files, _rid for reading and _wid
 *  for writing., _rfile and _wfile are shared pointers to FILE structs.
 *  _wid is also atomic so that read can read it without any protection.
 *
 *  _base_path is the base name of files, it may be followed by a number that
 *  is _rid or _wid.
 *
 *  _woffset and _roffset are offsets from the files begin to write or read.
 */
class splitter : public fs_file {
  bool _auto_delete;
  std::string _base_path;
  const uint32_t _max_file_size;

  std::shared_ptr<FILE> _rfile;
  int32_t _rid;
  long _roffset;

  std::mutex _write_m;
  std::shared_ptr<FILE> _wfile;
  std::atomic_int _wid;
  long _woffset;

  void _open_read_file();
  bool _open_write_file();

 public:
  splitter(const std::string& path,
           uint32_t max_file_size = 100000000u,
           bool auto_delete = false);
  ~splitter();
  splitter(const splitter&) = delete;
  splitter& operator=(const splitter&) = delete;
  void close() override final;
  long read(void* buffer, long max_size) override;
  void remove_all_files();
  void seek(long offset,
            fs_file::seek_whence whence = fs_file::seek_start) override;
  long tell() override;
  long write(void const* buffer, long size) override;
  void flush() override;

  std::string get_file_path(int id = 0) const;
  int32_t get_rid() const;
  long get_roffset() const;
  int32_t get_wid() const;
  long get_woffset() const;
  size_t max_file_size() const;
};
}  // namespace file

}  // namespace com::centreon::broker

#endif  // !CCB_FILE_SPLITTER_HH
