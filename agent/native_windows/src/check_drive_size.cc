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

#include "drive_size.hh"

namespace com::centreon::agent::check_drive_size_detail {

static const absl::flat_hash_map<std::string, e_drive_fs_type>
    _sz_filesystem_map = {{"fat", e_drive_fs_type::hr_fs_fat},
                          {"fat32", e_drive_fs_type::hr_fs_fat32},
                          {"ntfs", e_drive_fs_type::hr_fs_ntfs},
                          {"exfat", e_drive_fs_type::hr_fs_exfat}};

/**
 * @brief Get the type of drive and type of filesystem
 *
 * @param fs_root like C:\
 * @param logger
 * @return e_drive_fs_type
 */
static e_drive_fs_type get_fs_type(
    const std::string& fs_root,
    const std::shared_ptr<spdlog::logger>& logger) {
  // drive type
  uint64_t fs_type = e_drive_fs_type::hr_unknown;
  UINT drive_type = GetDriveTypeA(fs_root.c_str());
  switch (drive_type) {
    case DRIVE_FIXED:
      fs_type = e_drive_fs_type::hr_storage_fixed_disk;
      break;
    case DRIVE_REMOVABLE:
      fs_type = e_drive_fs_type::hr_storage_removable_disk;
      break;
    case DRIVE_REMOTE:
      fs_type = e_drive_fs_type::hr_storage_network_disk;
      break;
    case DRIVE_CDROM:
      fs_type = e_drive_fs_type::hr_storage_compact_disc;
      break;
    case DRIVE_RAMDISK:
      fs_type = e_drive_fs_type::hr_storage_ram_disk;
      break;
    default:
      fs_type = e_drive_fs_type::hr_unknown;
      SPDLOG_LOGGER_ERROR(logger, "{} unknown drive type {}", fs_root,
                          drive_type);
      break;
  }

  // format type
  char file_system_name[MAX_PATH];  // Tampon pour le nom du syst�me de
                                    // fichiers

  BOOL result =
      GetVolumeInformation(fs_root.c_str(), nullptr, 0, nullptr, nullptr,
                           nullptr, file_system_name, sizeof(file_system_name));

  if (!result) {
    SPDLOG_LOGGER_ERROR(logger, "{} unable to get file system type", fs_root);
  } else {
    std::string lower_fs_name = file_system_name;
    absl::AsciiStrToLower(&lower_fs_name);
    auto fs_search = _sz_filesystem_map.find(lower_fs_name);
    if (fs_search != _sz_filesystem_map.end()) {
      fs_type |= fs_search->second;
    } else {
      fs_type |= e_drive_fs_type::hr_fs_unknown;
      SPDLOG_LOGGER_ERROR(logger, "{} unknown file system type {}", fs_root,
                          file_system_name);
    }
  }
  return static_cast<e_drive_fs_type>(fs_type);
}

/**
 * @brief Get the used and total space of all drives allowed by filt
 *
 * @param filt fs filter (drive and fs type)
 * @param logger
 * @return std::list<fs_stat>
 */
std::list<fs_stat> os_fs_stats(filter& filt,
                               const std::shared_ptr<spdlog::logger>& logger) {
  DWORD drives = GetLogicalDrives();
  std::list<fs_stat> result;

  std::string fs_to_test;
  for (char letter = 'A'; letter <= 'Z'; ++letter) {
    // test if drive bit is set
    if (drives & (1 << (letter - 'A'))) {
      fs_to_test.clear();
      fs_to_test.push_back(letter);
      fs_to_test.push_back(':');
      fs_to_test.push_back('\\');

      // first use cache of filter
      if (filt.is_fs_yet_excluded(fs_to_test)) {
        continue;
      }

      if (!filt.is_fs_yet_allowed(fs_to_test)) {
        // not in cache so test it
        if (!filt.is_allowed(fs_to_test, "", get_fs_type(fs_to_test, logger))) {
          SPDLOG_LOGGER_TRACE(logger, "{} refused by filter", fs_to_test);
          continue;
        } else {
          SPDLOG_LOGGER_TRACE(logger, "{} allowed by filter", fs_to_test);
        }
      }

      ULARGE_INTEGER total_number_of_bytes;
      ULARGE_INTEGER total_number_of_free_bytes;

      BOOL success = GetDiskFreeSpaceEx(fs_to_test.c_str(), nullptr,
                                        &total_number_of_bytes,
                                        &total_number_of_free_bytes);

      if (success) {
        SPDLOG_LOGGER_TRACE(logger, "{} total: {}, free {}", fs_to_test,
                            total_number_of_bytes.QuadPart,
                            total_number_of_free_bytes.QuadPart);

        result.emplace_back(std::move(fs_to_test),
                            total_number_of_bytes.QuadPart -
                                total_number_of_free_bytes.QuadPart,
                            total_number_of_bytes.QuadPart);

      } else {
        SPDLOG_LOGGER_ERROR(logger, "unable to get free space of {}",
                            fs_to_test);
      }
    }
  }

  return result;
}

}  // namespace com::centreon::agent::check_drive_size_detail
