message(STATUS "Conan: Using CMakeDeps conandeps_legacy.cmake aggregator via include()")
message(STATUS "Conan: It is recommended to use explicit find_package() per dependency instead")

find_package(spdlog)
find_package(nlohmann_json)
find_package(asio)
find_package(CURL)

set(CONANDEPS_LEGACY  spdlog::spdlog  nlohmann_json::nlohmann_json  asio::asio  CURL::libcurl )