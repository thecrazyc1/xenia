/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <xenia/kernel/fs/devices/disc_image_file.h>

#include <algorithm>

#include <xenia/kernel/fs/device.h>
#include <xenia/kernel/fs/devices/disc_image_entry.h>
#include <xenia/kernel/fs/gdfx.h>

namespace xe {
namespace kernel {
namespace fs {

DiscImageFile::DiscImageFile(KernelState* kernel_state, Mode mode,
                             DiscImageEntry* entry)
    : XFile(kernel_state, mode), entry_(entry) {}

DiscImageFile::~DiscImageFile() { delete entry_; }

const std::string& DiscImageFile::path() const { return entry_->path(); }

const std::string& DiscImageFile::absolute_path() const {
  return entry_->absolute_path();
}

const std::string& DiscImageFile::name() const { return entry_->name(); }

X_STATUS DiscImageFile::QueryInfo(XFileInfo* out_info) {
  return entry_->QueryInfo(out_info);
}

X_STATUS DiscImageFile::QueryDirectory(XDirectoryInfo* out_info, size_t length,
                                       const char* file_name, bool restart) {
  return entry_->QueryDirectory(out_info, length, file_name, restart);
}

X_STATUS DiscImageFile::QueryVolume(XVolumeInfo* out_info, size_t length) {
  return entry_->device()->QueryVolume(out_info, length);
}

X_STATUS DiscImageFile::QueryFileSystemAttributes(
    XFileSystemAttributeInfo* out_info, size_t length) {
  return entry_->device()->QueryFileSystemAttributes(out_info, length);
}

X_STATUS DiscImageFile::ReadSync(void* buffer, size_t buffer_length,
                                 size_t byte_offset, size_t* out_bytes_read) {
  GDFXEntry* gdfx_entry = entry_->gdfx_entry();
  if (byte_offset >= gdfx_entry->size) {
    return X_STATUS_END_OF_FILE;
  }
  size_t real_offset = gdfx_entry->offset + byte_offset;
  size_t real_length = std::min(buffer_length, gdfx_entry->size - byte_offset);
  memcpy(buffer, entry_->mmap()->data() + real_offset, real_length);
  *out_bytes_read = real_length;
  return X_STATUS_SUCCESS;
}

}  // namespace fs
}  // namespace kernel
}  // namespace xe
