cmake_minimum_required(VERSION 3.18)
project(obs_screen_capture_node)

include(ExternalProject)

set(OBS_SOURCE_DIR ${CMAKE_BINARY_DIR}/obs-studio-src)
set(OBS_BUILD_DIR ${CMAKE_BINARY_DIR}/obs-studio-build)

ExternalProject_Add(obs_studio
  GIT_REPOSITORY https://github.com/obsproject/obs-studio.git
  GIT_TAG master
  SOURCE_DIR ${OBS_SOURCE_DIR}
  BINARY_DIR ${OBS_BUILD_DIR}
  UPDATE_COMMAND ""
  CMAKE_ARGS -DBUILD_SHARED_LIBS=ON -DENABLE_UI=OFF -DBUILD_TESTS=OFF -DBUILD_BROWSER=OFF -DENABLE_PLUGINS=OFF
  BUILD_COMMAND ${CMAKE_COMMAND} --build ${OBS_BUILD_DIR} --target libobs
  INSTALL_COMMAND ""
)

add_custom_target(node_addon ALL
  COMMAND npm install
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS obs_studio
)
