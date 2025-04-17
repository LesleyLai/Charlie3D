#include "file_watcher.hpp"

// TODO: implement file watcher in Linux

namespace charlie {

struct FileWatcherImpl {
  char padding;
};

FileWatcher::FileWatcher() = default;
FileWatcher::~FileWatcher() = default;

void FileWatcher::add_watch(FileWatchInfo&&) {}

void FileWatcher::poll_notifications() {}

} // namespace charlie
