find_package(ffmpeg REQUIRED)
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
