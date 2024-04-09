cmake_minimum_required(VERSION 3.12)
set(lib_name http_decoder)

project (${lib_name})


set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(Version)

add_definitions(-D_GNU_SOURCE)
add_definitions(-D__USE_MISC)
add_definitions(-D__FAVOR_BSD)
add_definitions(-D__USE_BSD)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
# 	set (CMAKE_INSTALL_PREFIX "/opt/tsg/" CACHE PATH "default install path" FORCE)
# endif()
set(CMAKE_INSTALL_PREFIX /opt/tsg/)

LINK_DIRECTORIES(/opt/MESA/lib /usr/lib64)

find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
if (CMAKE_CXX_CPPCHECK)
    list(
        APPEND CMAKE_CXX_CPPCHECK 
            "--enable=all"
            "--error-exitcode=1"
            "--suppress=unusedFunction"
            "--suppress=missingInclude"
            "--suppress=uselessAssignmentPtrArg"
            "--suppress=unmatchedSuppression"
            "--suppress=variableScope"
            "--suppress=unreadVariable"
            "--suppress=cstyleCast"
            "--suppress=memleakOnRealloc"
            "--suppress=constParameter"
            "--suppress=uselessAssignmentArg"
            "--suppress=uninitvar"
            "--suppress=unusedStructMember"
            "--suppress=unreachableCode"
            "--suppress=internalAstError"
            "--suppress=nullPointerRedundantCheck"
            "--suppress=ctunullpointer"
            "--suppress=redundantAssignment"
            "--suppress=duplicateValueTernary"
    )
    set(CMAKE_C_CPPCHECK ${CMAKE_CXX_CPPCHECK})
else()
  message(FATAL_ERROR "Could not find the program cppcheck.")
endif()

#ASAN option
set(ASAN_OPTION "OFF" CACHE STRING
    " set asan type chosen by the user, using OFF as default")
set_property(CACHE ASAN_OPTION  PROPERTY STRINGS OFF ADDRESS THREAD)
message(STATUS "ASAN_OPTION='${ASAN_OPTION}'")

if(ASAN_OPTION STREQUAL "ADDRESS")
    set(CMAKE_C_FLAGS "${CMAKADDRESS} -g -DCMAKE_BUILD_TYPE=Debug -fsanitize=address -fno-omit-frame-pointer")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DCMAKE_BUILD_TYPE=Debug -fsanitize=address -fno-omit-frame-pointer")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lasan")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lasan")
	add_definitions(-DASAN_ENABLED=1)
elseif(ASAN_OPTION STREQUAL "THREAD")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -DCMAKE_BUILD_TYPE=Debug -fsanitize=thread -fno-omit-frame-pointer")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DCMAKE_BUILD_TYPE=Debug -fsanitize=thread -fno-omit-frame-pointer")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lasan")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lasan")
	add_definitions(-DASAN_ENABLED=1)
endif()

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)

add_subdirectory(vendor)
add_subdirectory(src)

enable_testing()
add_subdirectory(test)

set(CPACK_RPM_LIBRARIES_USER_FILELIST "%config(noreplace) ${CMAKE_INSTALL_PREFIX}/sapp/etc/http/http_decoder.toml")

install(FILES conf/http_decoder.toml DESTINATION ${CMAKE_INSTALL_PREFIX}/sapp/etc/http COMPONENT PROFILE)
install(FILES include/http_decoder.h DESTINATION ${CMAKE_INSTALL_PREFIX}/framework/include/http_decoder COMPONENT HEADER)

SET(CPACK_RPM_LIBRARIES_PRE_INSTALL_SCRIPT_FILE "${PROJECT_SOURCE_DIR}/cmake/PreInstall.sh")
SET(CPACK_RPM_LIBRARIES_PRE_UNINSTALL_SCRIPT_FILE "${PROJECT_SOURCE_DIR}/cmake/PreUninstall.sh")
SET(CPACK_RPM_LIBRARIES_POST_INSTALL_SCRIPT_FILE "${PROJECT_SOURCE_DIR}/cmake/PostInstall.sh")
include(Package)