#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "../Charlie/utils/file_watcher.hpp"
#include <beyond/utils/assert.hpp>

const static auto test_folder = std::filesystem::current_path() / "temp" / "file_watcher_test";

struct FileActionCounter {
  int added = 0;
  int removed = 0;
  int modified = 0;
  int renamed_old = 0;
  int renamed_new = 0;

  friend auto operator==(FileActionCounter, FileActionCounter) -> bool = default;
  friend auto operator<<(std::ostream& os, const FileActionCounter& counter) -> std::ostream&
  {
    os << fmt::format(
        "{{.added = {}, .removed = {}, .modified = {}, .renamed_old = {}, .renamed_new = {}}}",
        counter.added, counter.removed, counter.modified, counter.renamed_old, counter.renamed_new);
    return os;
  }
};

TEST_CASE("Filewatcher test")
{
  if (std::filesystem::exists(test_folder)) { std::filesystem::remove_all(test_folder); }
  std::filesystem::create_directories(test_folder);

  const auto filewatcher_test_folder = test_folder / "filewatcher_test";
  BEYOND_ENSURE(not std::filesystem::exists(filewatcher_test_folder));
  std::filesystem::create_directories(filewatcher_test_folder);

  std::unordered_map<std::filesystem::path, FileActionCounter> counters_map;

  charlie::FileWatcher file_watcher;
  file_watcher.add_watch(
      {.path = filewatcher_test_folder,
       .callback = [&](const std::filesystem::path& path, charlie::FileAction event) {
         switch (event) {
         case charlie::FileAction::added:
           counters_map[path].added++;
           break;
         case charlie::FileAction::removed:
           counters_map[path].removed++;
           break;
         case charlie::FileAction::modified:
           counters_map[path].modified++;
           break;
         case charlie::FileAction::renamed_old:
           counters_map[path].renamed_old++;
           break;
         case charlie::FileAction::renamed_new:
           counters_map[path].renamed_new++;
           break;
         }
       }});

  const auto dir1_path = filewatcher_test_folder / "dir1";
  std::filesystem::create_directory(dir1_path);
  file_watcher.poll_notifications();
  REQUIRE(counters_map.at(dir1_path) == FileActionCounter{.added = 1});

  const auto file1_path = filewatcher_test_folder / "file1.txt";
  // Create
  {
    std::ofstream file{file1_path};
  }
  file_watcher.poll_notifications();
  REQUIRE(counters_map.at(file1_path) == FileActionCounter{.added = 1});

  // Modify
  {
    std::ofstream{file1_path} << "test\n";
  }
  file_watcher.poll_notifications();
  REQUIRE(counters_map.at(file1_path).added == 1);
  // Windows ReadDirectoryChangesW will create duplicated modification event
  REQUIRE(counters_map.at(file1_path).modified >= 1);

  file_watcher.poll_notifications();
}