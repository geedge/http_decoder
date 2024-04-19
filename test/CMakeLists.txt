set(DECODER_NAME http_decoder)

add_library(${DECODER_NAME}_test SHARED http_decoder_gtest.cpp md5.c)
add_dependencies(${DECODER_NAME}_test ${DECODER_NAME})
target_link_libraries(${DECODER_NAME}_test MESA_prof_load cjson-static)
set_target_properties(${DECODER_NAME}_test PROPERTIES PREFIX "")

add_library(${DECODER_NAME}_perf SHARED http_decoder_perf.cpp)
add_dependencies(${DECODER_NAME}_perf ${DECODER_NAME})
set_target_properties(${DECODER_NAME}_perf PROPERTIES PREFIX "")

set(TEST_RUN_DIR ${CMAKE_BINARY_DIR}/testing)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(/usr/local/include/cjson)
include_directories(/opt/tsg/framework/include/stellar)
include_directories(/opt/tsg/framework/include/)
include_directories(/opt/MESA/include/MESA)

add_executable(gtest_http_decoder http_gtest_main.cpp http_decoder_driver.cpp http_decoder_stub.cpp http_decoder_gtest.cpp md5.c base64.c)
link_directories(${CMAKE_BINARY_DIR}/src)
target_link_libraries(gtest_http_decoder http_decoder gtest pcap MESA_jump_layer cjson-static)

add_executable(gtest_unit http_gtest_unit.cpp http_decoder_driver.cpp http_decoder_stub.cpp http_decoder_gtest.cpp md5.c base64.c)
target_link_libraries(gtest_unit http_decoder gtest pcap MESA_jump_layer cjson-static)

set(TEST_MAIN gtest_http_decoder)

add_test(NAME CREATE_RUN_ENV COMMAND sh -c "mkdir -p ${TEST_RUN_DIR}")
add_test(NAME COPY_TEST_MAIN COMMAND sh -c "cp ${CMAKE_BINARY_DIR}/test/${TEST_MAIN} ${TEST_RUN_DIR}/${TEST_MAIN}")
add_test(NAME COPY_TEST_UNIT COMMAND sh -c "cp ${CMAKE_BINARY_DIR}/test/gtest_unit ${TEST_RUN_DIR}/gtest_unit")
add_test(NAME COPY_HTTP_DECODER_CONF COMMAND sh -c "mkdir -p ${TEST_RUN_DIR}/etc/http && cp ${PROJECT_SOURCE_DIR}/conf/http_decoder.toml ${TEST_RUN_DIR}/etc/http/")


set_tests_properties(CREATE_RUN_ENV COPY_TEST_MAIN COPY_TEST_UNIT COPY_HTTP_DECODER_CONF  
                     PROPERTIES FIXTURES_SETUP TestFixture)

# run tests
add_test(NAME HTTP_UNIT_TEST COMMAND ./gtest_unit WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_GET_SINGLE_TRANS_TEST COMMAND ${TEST_MAIN} -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_single_trans.json
         -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_get_single_trans.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_GET_SINGLE_TRANS_MSS1_TEST COMMAND ${TEST_MAIN} -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_single_trans.json
         -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_get_single_trans.pcap -m 1 WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_GET_MULTI_TRANS_TEST COMMAND ${TEST_MAIN} -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_multi_trans.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_get_multi_trans.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_GET_LONG_COOKIE_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_long_cookie.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_get_long_cookie.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_GET_ENCODED_URI_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_encoded_uri.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_get_encoded_uri.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_RES_GZIP_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_res_gzip.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_res_gzip.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_CHUNKED_RES_GZIP_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_chunked_res_gzip.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_chunked_res_gzip.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_OVER_TCP_KEEPALIVE_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_over_tcp_keepalive.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_over_tcp_keepalive.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

#todo: support http tunnel to fully parse pop3 protocol
add_test(NAME HTTP_TUNNEL_FOR_POP3_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_tunnel_for_pop3.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_tunnel_for_pop3.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_OVER_PPPOE_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_over_pppoe.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_over_pppoe.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_OVER_TLS_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_over_tls.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_over_tls.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME NON_HTTP_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/non_http.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/non_http.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_REQ_1BYTE_SLIDING_WINDOW_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_req_1byte_sliding_window.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_req_1byte_sliding_window.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_RES_1BYTE_SLIDING_WINDOW_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_res_1byte_sliding_window.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_res_1byte_sliding_window.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_NO_CONTENT_LENGTH_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_no_content_length.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_no_content_length.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_POST_MULTIPART_FORM_DATA_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_post_multipart_form_data.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_post_multipart_form_data.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_HEADERS_EXCEED_MAXIMUM_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_hdrs_exceed_maximum.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_hdrs_exceed_maximum.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

#add_test(NAME HTTP_CONNECT_FLOOD_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_connect_flood.json
#        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_connect_flood.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_GET_MALFORMED_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_malformed.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_get_malformed.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_HEADER_VALUE_EMPTY_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_hdr_value_empty.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_hdr_value_empty.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_UPGRADE_WEBSOCKET_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_upgrade_websocket.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_upgrade_websocket.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_UPGRADE_HTTP2_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_upgrade_http2.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_upgrade_http2.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_MULTI_PARSE_ERROR_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_multi_parse_error.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_multi_parse_error.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_GET_REQ_PIPELINE_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_req_pipeline.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_get_req_pipeline.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_TRANS_PIPELINE_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_trans_pipeline.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_trans_pipeline.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_HEADER_TRUNCATED_IN_KV_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_hdr_truncated_in_kv.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_hdr_truncated_in_kv.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_HEADER_TRUNCATED_AFTER_KV_TEST COMMAND ${TEST_MAIN} -b  ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_hdr_truncated_after_kv.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_hdr_truncated_after_kv.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_URL_WITH_HOST_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_url_test_with_host.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_url_test_with_host.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_URL_WITHOUT_HOST_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_url_test_without_host.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_url_test_without_host.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_6OVER4_SINGLE_TRANS_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_6over4_single_trans.json
        -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_6over4_single_trans.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_POST_SINGLE_TRANS_TEST COMMAND ${TEST_MAIN} -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_post_single_trans.json
         -p ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/http_post_single_trans.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_SELF_CONSISTENT_V4_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_single_trans.json
        -s ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_single_trans.json WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_SELF_CONSISTENT_V6_TEST COMMAND ${TEST_MAIN}  -b ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_6over4_single_trans.json
        -s ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_6over4_single_trans.json WORKING_DIRECTORY ${TEST_RUN_DIR})

set_tests_properties(HTTP_UNIT_TEST
                     HTTP_GET_SINGLE_TRANS_TEST
                     HTTP_GET_SINGLE_TRANS_MSS1_TEST
                     HTTP_GET_MULTI_TRANS_TEST
                     HTTP_GET_LONG_COOKIE_TEST
                     HTTP_GET_ENCODED_URI_TEST
                     HTTP_RES_GZIP_TEST
                     HTTP_CHUNKED_RES_GZIP_TEST
                     HTTP_OVER_TCP_KEEPALIVE_TEST
                     HTTP_TUNNEL_FOR_POP3_TEST
                     HTTP_OVER_PPPOE_TEST
                     HTTP_OVER_TLS_TEST
                     NON_HTTP_TEST
                     HTTP_REQ_1BYTE_SLIDING_WINDOW_TEST
                     HTTP_RES_1BYTE_SLIDING_WINDOW_TEST
                     HTTP_NO_CONTENT_LENGTH_TEST
                     HTTP_POST_MULTIPART_FORM_DATA_TEST
                     HTTP_HEADERS_EXCEED_MAXIMUM_TEST
                     HTTP_GET_MALFORMED_TEST
                     HTTP_HEADER_VALUE_EMPTY_TEST
                     HTTP_MULTI_PARSE_ERROR_TEST
                     HTTP_UPGRADE_WEBSOCKET_TEST
                     HTTP_UPGRADE_HTTP2_TEST
                     HTTP_GET_REQ_PIPELINE_TEST
                     HTTP_TRANS_PIPELINE_TEST
                     HTTP_HEADER_TRUNCATED_IN_KV_TEST
                     HTTP_HEADER_TRUNCATED_AFTER_KV_TEST
                     HTTP_URL_WITH_HOST_TEST
                     HTTP_URL_WITHOUT_HOST_TEST
                     HTTP_6OVER4_SINGLE_TRANS_TEST
                     HTTP_POST_SINGLE_TRANS_TEST
                     HTTP_SELF_CONSISTENT_V4_TEST
                     HTTP_SELF_CONSISTENT_V6_TEST
                     PROPERTIES FIXTURES_REQUIRED TestFixture)
