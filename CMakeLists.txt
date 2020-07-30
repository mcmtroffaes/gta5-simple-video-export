cmake_minimum_required (VERSION 2.6)
project(SimpleVideoExport)
set(CMAKE_CXX_STANDARD 17)

find_package(spdlog CONFIG REQUIRED)
find_package(ffmpeg REQUIRED)
find_library(VPX_LIBRARY NAMES vpxmd)
find_path(VPX_INCLUDE_DIR NAMES vpx/vpx_codec.h)
find_package(PolyHook_2 CONFIG REQUIRED)

include_directories("${PROJECT_SOURCE_DIR}/spdlog/include")
include_directories("${PROJECT_SOURCE_DIR}/inipp/inipp")
include_directories("${PROJECT_SOURCE_DIR}/common")
include_directories("${PROJECT_SOURCE_DIR}/plugin")

add_subdirectory(common)

add_executable(simple_video_export_test test/test.cpp)
target_link_libraries(simple_video_export_test PRIVATE common)
target_link_libraries(simple_video_export_test PRIVATE ${FFMPEG_LIBRARIES})
target_include_directories(simple_video_export_test PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(simple_video_export_test PRIVATE ${VPX_LIBRARY})
target_include_directories(simple_video_export_test PRIVATE ${VPX_INCLUDE_DIR})

add_library(SimpleVideoExport SHARED
  plugin/dllmain.cpp
  plugin/info.cpp
  plugin/sinkwriter.cpp)
set_target_properties(SimpleVideoExport PROPERTIES SUFFIX ".asi")
target_link_libraries(SimpleVideoExport PRIVATE common)
target_link_libraries(SimpleVideoExport PRIVATE ${FFMPEG_LIBRARIES})
target_include_directories(SimpleVideoExport PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(SimpleVideoExport PRIVATE ${VPX_LIBRARY})
target_include_directories(SimpleVideoExport PRIVATE ${VPX_INCLUDE_DIR})
target_link_libraries(SimpleVideoExport PRIVATE PolyHook_2::PolyHook_2)
