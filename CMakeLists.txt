cmake_minimum_required (VERSION 2.6)
project(SimpleVideoExport)
set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCODEC libavcodec REQUIRED)
pkg_check_modules(AVFORMAT libavformat REQUIRED)
pkg_check_modules(AVUTIL libavutil REQUIRED)
pkg_check_modules(AVSWRESAMPLE libswresample REQUIRED)
pkg_check_modules(AVSWSCALE libswscale REQUIRED)

include_directories(${AVCODEC_INCLUDE_DIRS})

include_directories("${PROJECT_SOURCE_DIR}/spdlog/include")
include_directories("${PROJECT_SOURCE_DIR}/inipp/inipp")
include_directories("${PROJECT_SOURCE_DIR}/common")

add_subdirectory(common)

add_executable(simple_video_export_test test/test.cpp)

target_link_libraries(simple_video_export_test PUBLIC
  common
  ${AVCODEC_LIBRARIES}
  ${AVFORMAT_LIBRARIES}
  ${AVUTIL_LIBRARIES}
  ${AVSWRESAMPLE_LIBRARIES}
  ${AVSWSCALE_LIBRARIES}
  )

target_link_options(simple_video_export_test PUBLIC
  ${AVCODEC_LDFLAGS}
  ${AVFORMAT_LDFLAGS}
  ${AVUTIL_LDFLAGS}
  ${AVSWRESAMPLE_LDFLAGS}
  ${AVSWSCALE_LDFLAGS}
  )
