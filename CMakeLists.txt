cmake_minimum_required (VERSION 2.8)


include(CMakeDependentOption)
include(ExternalProject)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Sets the configuration to build") # We will later change it to release
endif()

project(KeksNL)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT MSVC)
	SET(GCC_COVERAGE_COMPILE_FLAGS "-std=c++1y -g -pthread")
	SET(GCC_COVERAGE_LINK_FLAGS    "-pthread")

	SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}" )
	SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}" )
endif()

if(MSVC OR MSVC_IDE)
  if( MSVC_VERSION LESS 1800 )       # VC11-/VS2012-
    message(FATAL_ERROR "The project requires C++11/C++14 features. "
      "You need at least Visual Studio 12 (Microsoft Visual Studio 2013), "
      "with Microsoft Visual C++ Compiler Nov 2013 CTP (CTP_Nov2013).")
  elseif( MSVC_VERSION EQUAL 1800 )  # VC12/VS2013
	if(NOT CMAKE_GENERATOR_TOOLSET STREQUAL  "CTP_Nov2013")
		message( "VC12: use Microsoft Visual Studio 2013 "
		  "with Microsoft Visual C++ Compiler Nov 2013 CTP (CTP_Nov2013)" )
		set(CMAKE_GENERATOR_TOOLSET "CTP_Nov2013" CACHE STRING "Platform Toolset" FORCE)
	endif()
  else() # VC13+, assuming C++11/C++14  supported.
  endif()
endif()

find_package(Doxygen)

# THX to cegui for that
macro(_set_msvc_runtime_flags)
    if (KEKSNL_MSVC_STATIC_RUNTIME)
        foreach(_BUILD_CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            foreach(_BUILD_VARS CMAKE_CXX_FLAGS_${_BUILD_CONFIG} CMAKE_C_FLAGS_${_BUILD_CONFIG})
                string(REGEX REPLACE /MD /MT ${_BUILD_VARS} ${${_BUILD_VARS}})
            endforeach()
        endforeach()
    endif()
endmacro()

cmake_dependent_option( KEKSNL_MSVC_STATIC_RUNTIME "Specifies whether to the static runtime (/MT and /MTd) or the DLL runtime (/MD and /MDd).
NOTE: This will also affect which set of dependency libraries are linked with." FALSE "MSVC" FALSE )

option( KEKSNL_BUILD_STATIC_CONFIGURATION "Specifies whether the static configs will be built.  (NB: Shared configs are always built)" FALSE)


# Some windows specific stuff
if(WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNING -DNOMINMAX)

	# Setup some msvc stuff
	_set_msvc_runtime_flags()
endif()

# set build output locations
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})

if (WIN32)
    set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
else()
    set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endif()
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/debug)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/release)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/debug)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/release)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

ExternalProject_Add(KeksLogger
  GIT_REPOSITORY https://github.com/Crix-Dev/KeksLogger.git
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/vendor/KeksLogger
  INSTALL_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
)

file (GLOB CORE_HEADER_FILES include/*.h)
file (GLOB SOCKET_HEADER_FILES include/Sockets/*.h)

set(HEADER_FILES ${CORE_HEADER_FILES} ${SOCKET_HEADER_FILES})

include_directories(include
							include/Sockets
)

file (GLOB CORE_SOURCE_FILES src/*.cpp)
file (GLOB SOCKET_SOURCE_FILES src/Sockets/*.cpp)


source_group("Source Files"   FILES ${CORE_SOURCE_FILES})
source_group("Source Files\\Sockets"      FILES ${SOCKET_SOURCE_FILES})
source_group("Header Files"    FILES ${CORE_HEADER_FILES})
source_group("Header Files\\Sockets"  FILES ${SOCKET_HEADER_FILES})

set(SOURCE_FILES ${CORE_SOURCE_FILES} ${SOCKET_SOURCE_FILES})

add_executable(KeksNL ${SOURCE_FILES} ${HEADER_FILES})

add_dependencies(KeksNL KeksLogger)

if(WIN32)
  target_link_libraries(KeksNL ws2_32)
endif()

ExternalProject_Get_Property(KeksLogger install_dir)


# Link does not work the path i get is wrong/the install path given to ExternalProject_Add is not used, but i dont know why
#target_link_libraries(KeksNL ${install_dir}/lib/libKeksLogger.a)
