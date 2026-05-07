
macro(AddLibrarayToProjectInit)
  if (isSetAddLibrarayToProjectInit)
    #return()
  endif()

  set(isSetAddLibrarayToProjectInit TRUE CACHE INTERNAL "" FORCE)

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(LIBDIRSUFFIX "lib64")
  else()
    set(LIBDIRSUFFIX "lib")
  endif()

  set(ENV{PKG_CONFIG_PATH} "${CMAKE_BINARY_DIR}/root-install/${LIBDIRSUFFIX}/pkgconfig:$ENV{PKG_CONFIG_PATH}")
  set(ENV{LD_LIBRARY_PATH} "${CMAKE_BINARY_DIR}/root-install/${LIBDIRSUFFIX}:$ENV{LD_LIBRARY_PATH}")

  # remove this
  execute_process(COMMAND export PKG_CONFIG_PATH=$ENV{PKG_CONFIG_PATH})
  execute_process(COMMAND export LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH})

  ####set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/root-install CACHE INTERNAL "" FORCE)
  list(PREPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/root-install)
  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} CACHE INTERNAL "" FORCE)

endmacro()


macro(AddLibrarayToProject LibraryName)

  message("Add ${LibraryName} to project")

  execute_process(COMMAND ${CMAKE_COMMAND}
    -S ${AddLibrarayToProject_DIR}/${LibraryName}
    -B ${CMAKE_BINARY_DIR}/${LibraryName}-build
    -G ${CMAKE_GENERATOR}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/root-install
    -DEXTERNAL_PKG_CONFIG_PATH=${CURRENT_PKG_PATH}
  )

  execute_process(COMMAND ${CMAKE_COMMAND}
    --build ${CMAKE_BINARY_DIR}/${LibraryName}-build
    RESULT_VARIABLE result
  )

  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Build ${LibraryName} failed with error code ${result}.")
  endif()

  AddLibrarayToProjectInit()

endmacro()
