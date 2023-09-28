#include <catch2/catch_test_macros.hpp>

#include "../src/utils/thread_pool.hpp"

TEST_CASE("Thread pool tests")
{
  SECTION("An empty thread pool won't stuck")
  {
    charlie::ThreadPool thread_pool;
  }
}