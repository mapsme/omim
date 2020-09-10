function(run_bash)
  cmake_parse_arguments(ARG "" "CMD" "WORKING_DIRECTORY" ${ARGN})
  message("Run ${ARG_CMD} ...")
  execute_process(COMMAND bash -c ${ARG_CMD}
    WORKING_DIRECTORY ${ARG_WORKING_DIRECTORY}
    RESULT_VARIABLE ret)
  if (NOT (ret EQUAL "0"))
    message(FATAL_ERROR "Сommand ${ARG_CMD} failed with code ${ret}.")
  endif()
endfunction()

function(init_boost)
  run_bash(CMD "./bootstrap.sh" WORKING_DIRECTORY ${OMIM_BOOST_SRC})
endfunction()

function(boost_b2 option)
  run_bash(CMD "./b2 ${option}" WORKING_DIRECTORY ${OMIM_BOOST_SRC})
endfunction()

function(install_boost_headers)
  boost_b2("headers")
endfunction()

function(link_boost_lib exe library)
  boost_b2("install link=static --with-${library} --prefix=${OMIM_BOOST_BINARY_PATH}")
  find_package(Boost ${BOOST_VERSION} COMPONENTS ${library} REQUIRED)
  target_link_libraries(${exe} ${Boost_LIBRARIES})
endfunction()

function(setup_bundled_boost)
  find_package(Boost ${BOOST_VERSION} EXACT REQUIRED)
  include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
  set(Boost_LIBRARIES ${Boost_LIBRARIES} PARENT_SCOPE)
endfunction()

function(setup_bundled_boost_with_python)
  if (PYTHON_VERSION)
    string(REPLACE "." "" BOOST_PYTHON_LIBNAME ${PYTHON_VERSION})
    # Quote from https://cmake.org/cmake/help/v3.16/module/FindBoost.html:
    #   "Note that Boost Python components require a Python version
    #   suffix (Boost 1.67 and later), e.g. python36 or python27 for the
    #   versions built against Python 3.6 and 2.7, respectively."
    string(SUBSTRING ${BOOST_PYTHON_LIBNAME} 0 2 BOOST_PYTHON_LIBNAME)
    string(CONCAT BOOST_PYTHON_LIBNAME "python" ${BOOST_PYTHON_LIBNAME})
  endif()

  if (NOT DEFINED BOOST_PYTHON_LIBNAME)
    message(FATAL_ERROR "Could not determine Boost.Python library name from PYTHON_VERSION=" ${PYTHON_VERSION})
  else()
    message("Boost.Python library name: " ${BOOST_PYTHON_LIBNAME})
  endif()

  find_package(Boost ${BOOST_VERSION} EXACT REQUIRED COMPONENTS ${BOOST_PYTHON_LIBNAME})
  find_package(PythonInterp ${PYTHON_VERSION} REQUIRED)
  find_package(PythonLibs ${PYTHON_VERSION} REQUIRED)
  include_directories(${PYTHON_INCLUDE_DIRS})

  include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

  set(Boost_LIBRARIES ${Boost_LIBRARIES} PARENT_SCOPE)

  add_subdirectory(pyhelpers)
endfunction()

# End of functions.

set(BOOST_VERSION 1.68)
set(Boost_NO_SYSTEM_PATHS ON)
set(BOOST_ROOT "${OMIM_ROOT}/3party/boost")
set(BOOST_LIBRARYDIR "${BOOST_ROOT}/stage/lib")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)

if (PLATFORM_ANDROID)
  # todo This variable must be set by find_package(Boost).
  #      However, Android sdk that we currently use ships with CMake 3.10
  #      which is unable to do it because of a bug in CMake's find_path().
  set(Boost_INCLUDE_DIR ${BOOST_ROOT})
endif()

# TODO: Uncomment these lines when XCode project is finally generated by CMake.
# init_boost()
# install_boost_headers()

if (PYBINDINGS)
  setup_bundled_boost_with_python()
else()
  setup_bundled_boost()
endif()
