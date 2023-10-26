#include "file_watcher.hpp"

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

struct WatcherEntry : OVERLAPPED {
  std::filesystem::path directory_path;
  HANDLE directory_handle = nullptr;
  alignas(DWORD) BYTE buffer[4096 * 4] = {}; // 4 pages
  beyond::unique_function<void(const std::filesystem::path&)> callback;

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

    if (notify_info->Action != FILE_ACTION_MODIFIED) { continue; }

    int callback_filename_size = WideCharToMultiByte(
        CP_ACP, 0, notify_info->FileName, narrow<int>(notify_info->FileNameLength / sizeof(WCHAR)),
        callback_filename, MAX_PATH - 1, nullptr, nullptr);
    callback_filename[callback_filename_size] = '\0';

    entry.callback(entry.directory_path / callback_filename);
  } while (notify_info->NextEntryOffset != 0);

  refresh_watcher(entry);
}

static void refresh_watcher(WatcherEntry& entry)
{
  const bool result =
      ReadDirectoryChangesW(entry.directory_handle, entry.buffer, sizeof(entry.buffer), false,
                            FILE_NOTIFY_CHANGE_LAST_WRITE, nullptr, &entry, watch_callback);
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
  const auto directory = canonical(info.directory);

  BEYOND_ENSURE(exists(directory));
  BEYOND_ENSURE(is_directory(directory));

  if (auto itr = impl_->entries.find(directory); itr != impl_->entries.end()) {
    // TODO
    fmt::print("A watcher already exist!");
    return;
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
  entry.callback = std::move(info.callback);

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