cmake_minimum_required(VERSION 3.17)
cmake_policy(VERSION 3.17)
project(SimpleVideoExport)
set(CMAKE_CXX_STANDARD 17)

include_directories("${PROJECT_SOURCE_DIR}/inipp/inipp")
include_directories("${PROJECT_SOURCE_DIR}/common")
include_directories("${PROJECT_SOURCE_DIR}/plugin")

add_subdirectory(common)

add_executable(SimpleVideoExportTest test/test.cpp)
target_link_libraries(SimpleVideoExportTest PRIVATE common)

add_library(SimpleVideoExport SHARED
  plugin/dllmain.cpp
  plugin/info.cpp
  plugin/sinkwriter.cpp)
set_target_properties(SimpleVideoExport PROPERTIES SUFFIX ".asi")
target_link_libraries(SimpleVideoExport PRIVATE common)
target_link_directories(SimpleVideoExport PRIVATE ${CMAKE_SOURCE_DIR}/sdk/lib)
