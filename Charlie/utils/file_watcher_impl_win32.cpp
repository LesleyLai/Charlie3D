#include "file_watcher.hpp"

#include <variant>

#include <tracy/Tracy.hpp>

#define WIN32_LEAN_AND_MEAN
#include <strsafe.h>
#include <windows.h>

#include "../utils/prelude.hpp"
#include <beyond/utils/assert.hpp>

#include <algorithm>
#include <ranges>

static void error_exit(LPCSTR lpszFunction)
{
  // Retrieve the system error message for the last-error code
  LPVOID lpMsgBuf = nullptr;
  LPVOID lpDisplayBuf = nullptr;
  DWORD dw = GetLastError();

  FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, nullptr);

  // Display the error message and exit the process

  lpDisplayBuf = (LPVOID)LocalAlloc(
      LMEM_ZEROINIT,
      (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
  StringCchPrintf((LPTSTR)lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(TCHAR),
                  TEXT("%s failed with error %d: %s"), lpszFunction, dw, lpMsgBuf);

  LocalFree(lpMsgBuf);
  LocalFree(lpDisplayBuf);

  beyond::panic(fmt::format("{}", (LPCTSTR)lpDisplayBuf));
}

struct DirectoryWatcherData {
  charlie::FileWatcherCallback callback;
};

struct SingleFileWatchEntry {
  std::string filename;
  charlie::FileWatcherCallback callback;
};

struct SingleFileWatchersData {
  std::vector<SingleFileWatchEntry> files_to_watch;
};

struct WatcherEntry : OVERLAPPED {
  std::filesystem::path directory_path;
  HANDLE directory_handle = nullptr;
  alignas(DWORD) BYTE buffer[4096 * 4] = {}; // 4 pages
  std::variant<DirectoryWatcherData, SingleFileWatchersData> callbacks;

  [[nodiscard]] auto event() const -> HANDLE { return hEvent; }
};

static void refresh_watcher(WatcherEntry& entry);

static void CALLBACK watch_callback(DWORD error_code, DWORD number_of_bytes_transfered,
                                    OVERLAPPED* overlapped)
{
  if (number_of_bytes_transfered == 0) { return; }
  if (error_code != ERROR_SUCCESS) { return; }

  WatcherEntry& entry = *static_cast<WatcherEntry*>(overlapped);

  size_t offset = 0;
  PFILE_NOTIFY_INFORMATION notify_info{};
  char callback_filename[MAX_PATH];
  do {
    notify_info = (PFILE_NOTIFY_INFORMATION)&entry.buffer[offset];
    offset += notify_info->NextEntryOffset;

    const charlie::FileAction action = [&]() {
      switch (notify_info->Action) {
      case FILE_ACTION_ADDED:
        return charlie::FileAction::added;
      case FILE_ACTION_REMOVED:
        return charlie::FileAction::removed;
      case FILE_ACTION_MODIFIED:
        return charlie::FileAction::modified;
      case FILE_ACTION_RENAMED_OLD_NAME:
        return charlie::FileAction::renamed_old;
      case FILE_ACTION_RENAMED_NEW_NAME:
        return charlie::FileAction::renamed_new;
      }
      beyond::panic("Should not happen!");
    }();

    int callback_filename_size =
        WideCharToMultiByte(CP_ACP, 0, notify_info->FileName,
                            beyond::narrow<int>(notify_info->FileNameLength / sizeof(WCHAR)),
                            callback_filename, MAX_PATH - 1, nullptr, nullptr);
    callback_filename[callback_filename_size] = '\0';

    if (auto* callbacks = std::get_if<SingleFileWatchersData>(&entry.callbacks);
        callbacks != nullptr) {
      for (auto& [filename, callback] : callbacks->files_to_watch) {
        if (filename == callback_filename) {
          callback(entry.directory_path / callback_filename, action);
        }
      }
    } else {
      std::get<DirectoryWatcherData>(entry.callbacks)
          .callback(entry.directory_path / callback_filename, action);
    }

  } while (notify_info->NextEntryOffset != 0);

  refresh_watcher(entry);
}

static void refresh_watcher(WatcherEntry& entry)
{
  constexpr auto event_filters = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                 FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
                                 FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_LAST_ACCESS |
                                 FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SECURITY;

  const bool result =
      ReadDirectoryChangesW(entry.directory_handle, entry.buffer, sizeof(entry.buffer), false,
                            event_filters, nullptr, &entry, watch_callback);
  if (result == 0) {
    // Display the error message and exit the process
    error_exit(TEXT("ReadDirectoryChangesW"));
  }
}

namespace charlie {

struct FileWatcherImpl {
  std::unordered_map<std::filesystem::path, WatcherEntry> entries;

  // Only used in .poll_notifications()
  // Put it here to prevent allocation/deallocation every frame
  std::vector<HANDLE> events;
};

FileWatcher::FileWatcher() : impl_(std::make_unique<FileWatcherImpl>()) {}
FileWatcher::~FileWatcher() = default;

void FileWatcher::add_watch(FileWatchInfo&& info)
{
  ZoneScoped;

  const auto path = canonical(info.path);

  BEYOND_ENSURE(exists(path));

  std::filesystem::path directory;
  bool is_single_file_watcher = false;
  if (is_directory(path)) { // Directory watcher
    directory = path;
    if (impl_->entries.contains(directory)) {
      // A watcher already exist for this directory
      beyond::panic(fmt::format("A watcher already exist for {}!", directory.string()));
    }
  } else if (is_regular_file(path)) {
    is_single_file_watcher = true;
    directory = path;
    directory.remove_filename();
    if (auto itr = impl_->entries.find(directory); itr != impl_->entries.end()) {
      auto* data = std::get_if<SingleFileWatchersData>(&itr->second.callbacks);

      BEYOND_ENSURE_MSG(data != nullptr, fmt::format("A directory watcher already exist for {}!",
                                                     directory.string()));

      data->files_to_watch.push_back({
          .filename = path.filename().string(),
          .callback = BEYOND_MOV(info.callback),
      });
      return;
    }
  } else {
    beyond::panic(fmt::format("FileWatcher: Unsupported file status of path: {}", path.string()));
  }

  auto directory_handle =
      CreateFile(directory.string().c_str(), FILE_LIST_DIRECTORY,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                 FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
  if (directory_handle == INVALID_HANDLE_VALUE) // NOLINT(performance-no-int-to-ptr)
  {
    error_exit("CreateFile");
  }

  WatcherEntry entry{};
  entry.hEvent = CreateEvent(nullptr, true, false, nullptr);
  entry.directory_path = directory;
  entry.directory_handle = directory_handle;
  if (is_single_file_watcher) {
    auto callbacks = SingleFileWatchersData{};
    callbacks.files_to_watch.push_back(SingleFileWatchEntry{.filename = path.filename().string(),
                                                            .callback = BEYOND_MOV(info.callback)});
    entry.callbacks = BEYOND_MOV(callbacks);
  } else {
    entry.callbacks = DirectoryWatcherData{.callback = BEYOND_MOV(info.callback)};
  }

  auto [itr, inserted] = impl_->entries.try_emplace(directory, std::move(entry));
  BEYOND_ASSERT(inserted);
  refresh_watcher(itr->second);
}

void FileWatcher::poll_notifications()
{
  impl_->events.clear();
  std::ranges::transform(std::views::values(impl_->entries), std::back_inserter(impl_->events),
                         &WatcherEntry::event);

  MsgWaitForMultipleObjectsEx(narrow<DWORD>(impl_->events.size()), impl_->events.data(), 0,
                              QS_ALLINPUT, MWMO_ALERTABLE);
}

} // namespace charlie