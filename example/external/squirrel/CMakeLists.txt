cmake_minimum_required(VERSION 3.0)

project(squirrel)

include(squirrel.conf.cmake)

include(ExternalProject)

ExternalProject_Add(${PROJECT_NAME}
  #URL          ${CMAKE_CURRENT_LIST_DIR}/squirrel-${SQUIRREL_VERSION}.tar.gz
  #URL          https://github.com/albertodemichelis/squirrel/archive/refs/tags/v${SQUIRREL_VERSION}.tar.gz
  GIT_REPOSITORY https://github.com/albertodemichelis/squirrel.git
  GIT_TAG        6ad6f6427157757c4431907dde0a1e5712a5cf67

  SOURCE_DIR   ${CMAKE_BINARY_DIR}/source/${PROJECT_NAME}
  CMAKE_ARGS   ${SQUIRREL_CONF}
  CMAKE_GENERATOR "Unix Makefiles"
  #BUILD_COMMAND   make
  BUILD_COMMAND   ""
  INSTALL_COMMAND make VERBOSE=0 -j4 install
  PATCH_COMMAND patch -d <SOURCE_DIR> -p1 < ${CMAKE_CURRENT_LIST_DIR}/patch/01_meta_method.patch
        COMMAND patch -d <SOURCE_DIR> -p1 < ${CMAKE_CURRENT_LIST_DIR}/patch/03_add_gdb3.patch
)
