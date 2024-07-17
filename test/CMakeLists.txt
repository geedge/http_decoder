set(DECODER_NAME http_decoder)

aux_source_directory(${PROJECT_SOURCE_DIR}/deps/toml DEPS_SRC)

add_library(${DECODER_NAME}_test SHARED http_decoder_test_plug.cpp md5.c ${DEPS_SRC})
add_dependencies(${DECODER_NAME}_test ${DECODER_NAME})
target_link_libraries(${DECODER_NAME}_test MESA_prof_load cjson-static)
set_target_properties(${DECODER_NAME}_test PROPERTIES PREFIX "")

add_library(${DECODER_NAME}_perf SHARED http_decoder_perf_plug.cpp)
add_dependencies(${DECODER_NAME}_perf ${DECODER_NAME})
set_target_properties(${DECODER_NAME}_perf PROPERTIES PREFIX "")

set(TEST_RUN_DIR /home/mesasoft/sapp_run)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/deps)
include_directories(/opt/tsg/framework/include/)
include_directories(/opt/MESA/include/MESA)
include_directories(${CMAKE_BINARY_DIR}/vendor/vbuild/include)
include_directories(${CMAKE_BINARY_DIR}/vendor/cjson/src/cjson/include)

aux_source_directory(${PROJECT_SOURCE_DIR}/deps/mempool PERF_TEST_DEP_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/deps/toml PERF_TEST_DEP_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/src PERF_TEST_DEP_SRC)

add_executable(httpd_perf_test ${PERF_TEST_DEP_SRC} http_decoder_perf_main.cpp http_decoder_perf_plug.cpp http_stellar_mock.cpp)
target_link_libraries(httpd_perf_test z brotlidec llhttp-static fieldstat4 pthread)


add_executable(httpd_gtest http_decoder_gtest.cpp http_stellar_mock.cpp ${PROJECT_SOURCE_DIR}/src/http_decoder_utils.cpp)
target_link_libraries(httpd_gtest gtest)
