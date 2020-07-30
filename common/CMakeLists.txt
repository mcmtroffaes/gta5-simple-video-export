find_package(ffmpeg REQUIRED)
find_package(spdlog CONFIG REQUIRED)
add_library(common STATIC
  audiostream.cpp
  avcreate.cpp
  format.cpp
  logger.cpp
  settings.cpp
  stream.cpp
  videostream.cpp)
target_link_libraries(common PRIVATE ${FFMPEG_LIBRARIES})
target_include_directories(common PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(common PRIVATE PolyHook_2::PolyHook_2)
target_link_libraries(common PRIVATE spdlog::spdlog spdlog::spdlog_header_only)
