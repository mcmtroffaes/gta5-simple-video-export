find_package(spdlog CONFIG REQUIRED)
find_package(FFMPEG REQUIRED)
find_package(PolyHook_2 CONFIG REQUIRED)
add_library(common STATIC
  audiostream.cpp
  avcreate.cpp
  format.cpp
  logger.cpp
  settings.cpp
  stream.cpp
  videostream.cpp)
target_link_libraries(common PUBLIC ${FFMPEG_LIBRARIES})
target_include_directories(common PUBLIC ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(common PUBLIC PolyHook_2::PolyHook_2)
target_link_libraries(common PUBLIC spdlog::spdlog)
