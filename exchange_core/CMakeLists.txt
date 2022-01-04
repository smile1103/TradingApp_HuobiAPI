cmake_minimum_required(VERSION 3.12)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# version (using git tag)

#include(GetGitRepoVersion)

#message("Using GIT_REPO_VERSION=${GIT_REPO_VERSION}")

# project

#project(exchange-core VERSION ${GIT_REPO_VERSION})
project(exchange-core VERSION 1.0)


# conda

if(DEFINED ENV{CONDA_PREFIX} AND NOT DEFINED ENV{CONDA_BUILD})
  message("Setting CMAKE_INSTALL_PREFIX and CMAKE_FIND_ROOT_PATH")
  set(CMAKE_INSTALL_PREFIX $ENV{CONDA_PREFIX})
  set(CMAKE_FIND_ROOT_PATH $ENV{CONDA_PREFIX})
endif()

# c++

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# filesystem

include(GNUInstallDirs)

# dependencies

#find_package(Flatbuffers REQUIRED)
#find_package(PythonInterp REQUIRED)
#find_package(fmt REQUIRED)

#find_program(FLATBUFFERS_COMPILER flatc REQUIRED)
#find_program(CLANG_FORMAT clang-format REQUIRED)

# includes

include_directories("${CMAKE_SOURCE_DIR}/include" "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_INCLUDEDIR}")


# sub-projects

#add_subdirectory("${CMAKE_SOURCE_DIR}/schema/cpp")
#add_subdirectory("${CMAKE_SOURCE_DIR}/schema/fbs")

#add_subdirectory("${CMAKE_SOURCE_DIR}/include/roq")
#add_subdirectory("${CMAKE_SOURCE_DIR}/include/roq/fbs")

# project

add_library(${PROJECT_NAME} INTERFACE)

#add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}-schema-cpp)
#add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}-schema-fbs)

#add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}-include-cpp)
#add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}-include-fbs)

# install (public headers)

install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/include/exchange-core/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/exchange-core"
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "CMakeFiles" EXCLUDE)

# install (cmake)

install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}-config)

set(CMAKE_LIB_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

export(
  TARGETS ${PROJECT_NAME}
  NAMESPACE ${PROJECT_NAME}::
  FILE "${CMAKE_LIB_DIR}/${PROJECT_NAME}-config.cmake")

install(
  EXPORT ${PROJECT_NAME}-config
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION "${CMAKE_LIB_DIR}")