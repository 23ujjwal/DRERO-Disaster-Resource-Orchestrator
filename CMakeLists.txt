cmake_minimum_required(VERSION 3.16)
project(DRERO CXX)
set(CMAKE_CXX_STANDARD 17)

include_directories(core/include)
include_directories(/usr/include/cppconn)

# core: only free-function .cpp files (Database is fully inline in database.h)
set(CORE_SRC
    core/src/drero_config.cpp
    core/src/models.cpp
    core/src/scheduling.cpp
    core/src/graph.cpp
)
add_library(drero_core STATIC ${CORE_SRC})
target_link_libraries(drero_core mysqlcppconn pthread)

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

add_executable(drero_server backend/src/server.cpp)
target_link_libraries(drero_server drero_core cpp-httplib pthread)
