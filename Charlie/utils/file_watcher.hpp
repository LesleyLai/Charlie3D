#ifndef CHARLIE3D_FILE_WATCHER_HPP
#define CHARLIE3D_FILE_WATCHER_HPP

#include <beyond/utils/unique_function.hpp>
#include <filesystem>
#include <memory>

namespace charlie {

struct FileWatchInfo {
  std::filesystem::path directory;
  beyond::unique_function<void(const std::filesystem::path&)> callback;
};

class FileWatcher {
  std::unique_ptr<struct FileWatcherImpl> impl_;

public:
  FileWatcher();
  ~FileWatcher();

  FileWatcher(const FileWatcher&) = delete;
  auto operator=(const FileWatcher&) -> FileWatcher& = delete;

  void add_watch(FileWatchInfo&& info);

  void poll_notifications();
};

} // namespace charlie

#endif // CHARLIE3D_FILE_WATCHER_HPP
