#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "../src/utils/configuration.hpp"

TEST_CASE("Configurations Test")
{
  Configurations configs;

  configs.set("test", uint32_t{42});
  REQUIRE(configs.get<uint32_t>("test") == 42);

  configs.set("test2", std::string{"hello"});
  REQUIRE(configs.get<std::string>("test2") == "hello");
}