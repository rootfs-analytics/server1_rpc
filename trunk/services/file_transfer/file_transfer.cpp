#include "services/file_transfer/file_transfer.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
FileTransfer::FileTransfer() {
}

void FileTransfer::Start(int threads) {
}

FileTransfer *FileTransfer::Create(
    const string &host, const string &port,
    const string &filename, int threads) {
  FileTransfer *file_tranfer = new FileTransfer;
  FileTransfer::CheckBook *checkbook = &file_transfer.checkbook_;
  FileTransfer::MetaData *meta = checkbook->mutable_meta();
  meta->set_host(host);
  meta->set_port(port);
  meta->set_filename(filename);
  meta->set_threads(threads);
  boost::filesystem::path p(filename, boost::filesystem::native);
  if (!boost::filesystem::exists(p)) {
    LOG(WARNING) << "Not find: " << filename;
    return NULL;
  }
  if (!boost::filesystem::is_regular(p)) {
    LOG(WARNING) << "Not a regular file: " << filename;
    return NULL;
  }
  uint32 adler = adler32(0L, Z_NULL, 0);
  boost::iostreams::mapped_file_source file;
  if (!file.open(filename)) {
    LOG(WARNING) << "Fail to open file: " << filename;
    return NULL;
  }
  const char *data = file.data();
  int file_size = file.size();
  int slice_number = (file_size + kSliceSize - 1) / kSliceSize;
  const int odd = file_size - (slice_number - 1) * kSliceSize;
  uint32 adler = adler32(0L, Z_NULL, 0);
  uint32 previous_adler = adler;
  for (int i = 0; i < slice_number; ++i) {
    FileTransfer::Slice *slice = checkbook->add_slice();
    slice->set_index(i);
    int length = kSliceSize;
    if (i == slice_number - 1) {
      length = odd;
      slice->set_length(length);
    }
    previous_adler = adler;
    adler = adler32(adler, data, length);
    slice->set_adler(adler);
  }
  return file_transfer_;
}

FileTransfer *FileTranfser::Resume(const string &checkbook) {
  FileTransfer *file_tranfser = new FileTransfer;
  fstream input(checkbook, ios::in | ios::binary);
  if (!file_transfer_->check_book_.ParseFromIstream(&input)) {
    LOG(WARNING) << "Fail to parse checkbook: " << checkbook;
    return NULL;
  }
  return file_transfer;
}
