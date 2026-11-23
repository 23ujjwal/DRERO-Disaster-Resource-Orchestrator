cmake_minimum_required(VERSION 3.16)
project(DRERO CXX)
find_package(OpenSSL REQUIRED)
set(CMAKE_CXX_STANDARD 17)

include_directories(core/include)
include_directories(/usr/include/cppconn)

# Core algorithms and database modules
set(CORE_SRC
    core/src/database_connection.cpp
    core/src/database_requests.cpp
    core/src/database_resources.cpp
    core/src/database_reporting.cpp
    core/src/database_catalog.cpp
    core/src/auth.cpp
    core/src/drero_config.cpp
    core/src/models.cpp
    core/src/scheduling.cpp
    core/src/graph.cpp
)
add_library(drero_core STATIC ${CORE_SRC})
target_link_libraries(drero_core mysqlcppconn pthread OpenSSL::Crypto)

add_executable(drero_cli cli/src/main.cpp)
target_link_libraries(drero_cli drero_core)

add_executable(test_scheduling tests/test_scheduling.cpp)
target_link_libraries(test_scheduling drero_core)

add_executable(test_allocation_matching tests/test_allocation_matching.cpp)
target_link_libraries(test_allocation_matching drero_core)

add_executable(test_concurrency tests/test_concurrency.cpp)
target_link_libraries(test_concurrency drero_core)

add_executable(test_rag tests/test_rag.cpp)
target_link_libraries(test_rag drero_core)

add_executable(drero_server
    backend/src/server.cpp
    backend/src/api_auth.cpp
    backend/src/api_helpers.cpp
    backend/src/routes_public.cpp
    backend/src/routes_dispatcher.cpp
    backend/src/routes_scheduling.cpp
    backend/src/routes_administration.cpp
    backend/src/routes_operations.cpp
)
target_include_directories(drero_server PRIVATE backend/include)
target_link_libraries(drero_server drero_core cpp-httplib pthread)

add_executable(test_incremental_features tests/test_incremental_features.cpp)
target_link_libraries(test_incremental_features drero_core)

add_executable(test_persistent_sessions tests/test_persistent_sessions.cpp)
target_link_libraries(test_persistent_sessions drero_core)

add_executable(test_optional_core tests/test_optional_core.cpp)
target_link_libraries(test_optional_core drero_core)
