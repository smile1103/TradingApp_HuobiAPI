cmake_minimum_required(VERSION 3.12)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# version (using git tag)

#include(GetGitRepoVersion)

#message("Using GIT_REPO_VERSION=${GIT_REPO_VERSION}")

# project

#project(exchange-core VERSION ${GIT_REPO_VERSION})
project(exchange-phemex VERSION 1.0)


# conda

if(DEFINED ENV{CONDA_PREFIX} AND NOT DEFINED ENV{CONDA_BUILD})
  message("Setting CMAKE_INSTALL_PREFIX and CMAKE_FIND_ROOT_PATH")
  set(CMAKE_INSTALL_PREFIX $ENV{CONDA_PREFIX})
  set(CMAKE_FIND_ROOT_PATH $ENV{CONDA_PREFIX})
endif()

# c++
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)


# filesystem

include(GNUInstallDirs)

# dependencies

find_package(exchange-core REQUIRED)
find_package(spdlog REQUIRED)
#find_package(gflags REQUIRED)
#find_package(glog REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)
find_package(RapidJSON REQUIRED)
#find_package(websockets REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(OpenSSL REQUIRED)

include_directories(/usr/local/opt/openssl/include)
include_directories("${CMAKE_SOURCE_DIR}/include")


# sub-projects

add_subdirectory("${CMAKE_SOURCE_DIR}/test")

# project

#add_library(${PROJECT_NAME} INTERFACE)

#add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}-schema-cpp)
#add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}-schema-fbs)

#add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}-include-cpp)
#add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}-include-fbs)

# install (public headers)

install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/include/exchange-phemex/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/exchange-phemex"
  FILES_MATCHING
  PATTERN "*.h*"
  PATTERN "CMakeFiles" EXCLUDE)

# install (cmake)




