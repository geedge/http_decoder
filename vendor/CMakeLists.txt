include(ExternalProject)

set(VENDOR_BUILD ${CMAKE_BINARY_DIR}/vendor/vbuild)

# GoogleTest
ExternalProject_Add(googletest PREFIX googletest
        URL ${CMAKE_CURRENT_SOURCE_DIR}/googletest-release-1.8.0.tar.gz
        URL_MD5 16877098823401d1bf2ed7891d7dce36
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})

ExternalProject_Get_Property(googletest INSTALL_DIR)
file(MAKE_DIRECTORY ${INSTALL_DIR}/include)

add_library(gtest STATIC IMPORTED GLOBAL)
add_dependencies(gtest googletest)
set_property(TARGET gtest PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libgtest.a)
set_property(TARGET gtest PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)
set_property(TARGET gtest PROPERTY INTERFACE_LINK_LIBRARIES pthread)

add_library(gmock STATIC IMPORTED GLOBAL)
add_dependencies(gmock googletest)
set_property(TARGET gmock PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libgmock.a)
set_property(TARGET gmock PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)

#llhttp-9.1.3
ExternalProject_Add(llhttp PREFIX llhttp
                    URL ${CMAKE_CURRENT_SOURCE_DIR}/llhttp-release-v9.1.3.tar.gz
                    URL_MD5 f15cd242f55a38b30d3031b560d2c8ef
		    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${VENDOR_BUILD} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_C_FLAGS="-fPIC")

file(MAKE_DIRECTORY ${VENDOR_BUILD}/include)

add_library(llhttp-static STATIC IMPORTED GLOBAL)
add_dependencies(llhttp-static llhttp)

set_property(TARGET llhttp-static  PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${VENDOR_BUILD}/include)
set_property(TARGET llhttp-static  PROPERTY IMPORTED_LOCATION ${VENDOR_BUILD}/lib64/libllhttp.a)


#libcjson-1.7.17
ExternalProject_Add(cjson PREFIX cjson
                        URL ${CMAKE_CURRENT_SOURCE_DIR}/libcjson_v1.7.17.tar.gz
                        URL_MD5 4b2ab12cf065c079004aa44495ade04a
                        CONFIGURE_COMMAND ""
                        BUILD_COMMAND make 
                        INSTALL_COMMAND make install
                        BUILD_IN_SOURCE 1)

ExternalProject_Get_Property(cjson INSTALL_DIR)
file(MAKE_DIRECTORY ${VENDOR_BUILD}/include/cjson)

add_library(cjson-static STATIC IMPORTED GLOBAL)
add_dependencies(cjson-static cjson)
set_property(TARGET cjson-static PROPERTY IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/vendor/cjson/src/cjson/libcjson.a)
set_property(TARGET cjson-static PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${VENDOR_BUILD}/include/cjson)

