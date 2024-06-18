set(DECODER_NAME http_decoder)

set(TEST_RUN_DIR /home/mesasoft/sapp_run)
set(SAPP_DEVEL_DIR /home/mesasoft/sapp_run/lib)
set(STELLAR_ON_SAPP_LIB_DIR /opt/tsg/sapp/plug/stellar_on_sapp)
set(TEST_MAIN plugin_test_main)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/test)
include_directories(/usr/local/include/cjson)
include_directories(/opt/tsg/framework/include/stellar)
include_directories(/opt/MESA/include/MESA)

#various ways to add -rdynamic for centos7, centos8, and different cmake version
add_definitions(-rdynamic)
add_executable(plugin_test_main plugin_test_main.cpp)
set_target_properties(plugin_test_main 
    PROPERTIES 
    LINK_OPTIONS 
    "-rdynamic" 
    )
set_target_properties(plugin_test_main 
    PROPERTIES 
    LINK_FLAGS  
    "-rdynamic" 
    )
set(LINK_FLAGS "-rdynamic")
link_directories(${SAPP_DEVEL_DIR}/lib)
target_link_libraries(plugin_test_main gtest cjson-static sapp_devel pthread pcap MESA_htable MESA_field_stat2 MESA_htable MESA_prof_load MESA_handle_logger MESA_jump_layer breakpad_mini fieldstat3 fieldstat4 systemd )

# assemble test env
add_test(NAME STELLAR_INSTALL_TEST_MAIN COMMAND sh -c "cp ${CMAKE_CURRENT_BINARY_DIR}/${TEST_MAIN} ${TEST_RUN_DIR}/${TEST_MAIN}")
add_test(NAME STELLAR_COPY_SPEC COMMAND sh -c "mkdir -p ${TEST_RUN_DIR}/stellar_plugin/ && cp ${CMAKE_CURRENT_SOURCE_DIR}/env/spec.toml ${TEST_RUN_DIR}/stellar_plugin/spec.toml")
add_test(NAME STELLAR_COPY_CONFLIST COMMAND sh -c "mkdir -p ${TEST_RUN_DIR}/plug/ && cp ${CMAKE_CURRENT_SOURCE_DIR}/env/conflist.inf ${TEST_RUN_DIR}/plug/conflist.inf")
add_test(NAME STELLAR_COPY_START_LOADER_INF COMMAND sh -c "mkdir -p ${TEST_RUN_DIR}/plug/stellar_on_sapp && cp ${CMAKE_CURRENT_SOURCE_DIR}/env/start_loader.inf ${TEST_RUN_DIR}/plug/stellar_on_sapp/start_loader.inf")
add_test(NAME STELLAR_COPY_DEFER_LOADER_INF COMMAND sh -c "mkdir -p ${TEST_RUN_DIR}/plug/stellar_on_sapp && cp ${CMAKE_CURRENT_SOURCE_DIR}/env/defer_loader.inf ${TEST_RUN_DIR}/plug/stellar_on_sapp/defer_loader.inf")
add_test(NAME STELLAR_COPY_HTTP_DECODER_CONF COMMAND sh -c "mkdir -p ${TEST_RUN_DIR}/etc/http && cp ${PROJECT_SOURCE_DIR}/conf/http_decoder.toml ${TEST_RUN_DIR}/etc/http/")
add_test(NAME STELLAR_COPY_HTTP_GTEST_ENTRY_CONF COMMAND sh -c "mkdir -p ${TEST_RUN_DIR}/etc/http && cp ${PROJECT_SOURCE_DIR}/conf/gtest_entry.toml ${TEST_RUN_DIR}/etc/http/")

# update config files
add_test(NAME STELLAR_UPDATE_SAPP_LOG_LEVEL COMMAND bash -c "sed -i 's/sapp_log.fatal/sapp_log.info/' ${TEST_RUN_DIR}/etc/sapp_log.conf")
add_test(NAME STELLAR_UPDATE_SAPP_SYN_MODE COMMAND bash -c "sed -i 's/syn_mandatory=1/syn_mandatory=0/' ${TEST_RUN_DIR}/etc/sapp.toml")
add_test(NAME STELLAR_ENABLE_SAPP_MONITOR COMMAND bash -c "sed -i 's/monitor_thread_enabled=0/monitor_thread_enabled=1/' ${TEST_RUN_DIR}/etc/sapp.toml")
add_test(NAME UPDATE_GTEST_PLUG_ENTRY COMMAND bash -c "sed -i 's/name=.*/name=\\x22http_decoder_test_entry\\x22/' ${TEST_RUN_DIR}/etc/http/gtest_entry.toml")
add_test(NAME UPDATE_GTEST_PLUG_TOPIC COMMAND bash -c "sed -i 's/topic=.*/topic=\\x22HTTP_DECODER_MESSAGE\\x22/' ${TEST_RUN_DIR}/etc/http/gtest_entry.toml")

# update plugin to be tested
# add_test(NAME STELLAR_ON_SAPP_SO COMMAND sh -c "cp ${STELLAR_ON_SAPP_LIB_DIR}/stellar_on_sapp.so ${TEST_RUN_DIR}/plug/stellar_on_sapp/stellar_on_sapp.so")
add_test(NAME STELLAR_ON_SAPP_SO COMMAND sh -c "rpm -ql stellar-on-sapp | grep stellar_on_sapp.so | xargs -i cp -f {} ${TEST_RUN_DIR}/plug/stellar_on_sapp/stellar_on_sapp.so")
add_test(NAME STELLAR_HTTP_DECODER_SO COMMAND sh -c "cp ${CMAKE_BINARY_DIR}/src/${DECODER_NAME}.so ${TEST_RUN_DIR}/stellar_plugin/${DECODER_NAME}.so")
add_test(NAME STELLAR_HTTP_DECODER_GTEST_SO COMMAND sh -c "cp ${CMAKE_BINARY_DIR}/test/${DECODER_NAME}_test.so ${TEST_RUN_DIR}/stellar_plugin/${DECODER_NAME}_test.so")

set_tests_properties(STELLAR_INSTALL_TEST_MAIN  STELLAR_COPY_SPEC STELLAR_COPY_CONFLIST STELLAR_COPY_START_LOADER_INF STELLAR_COPY_DEFER_LOADER_INF STELLAR_COPY_HTTP_DECODER_CONF  STELLAR_COPY_HTTP_GTEST_ENTRY_CONF 
                    STELLAR_ON_SAPP_SO STELLAR_HTTP_DECODER_SO STELLAR_HTTP_DECODER_GTEST_SO STELLAR_UPDATE_SAPP_LOG_LEVEL STELLAR_UPDATE_SAPP_SYN_MODE STELLAR_ENABLE_SAPP_MONITOR 
                    UPDATE_GTEST_PLUG_ENTRY  UPDATE_GTEST_PLUG_TOPIC
                    PROPERTIES FIXTURES_SETUP TestFixture)

set(TEST_JSON_DIR ${PROJECT_SOURCE_DIR}/test/test_result_json)        
set(TEST_PCAP_DIR ${PROJECT_SOURCE_DIR}/test/http_pcap)

# run tests
add_test(NAME STELLAR_HTTP_GET_SINGLE_TRANS_TEST COMMAND ./${TEST_MAIN} ${TEST_JSON_DIR}/http_get_single_trans.json 
         -r ${TEST_PCAP_DIR}/http_get_single_trans.pcap WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_GET_MULTI_TRANS_TEST COMMAND ./${TEST_MAIN} ${TEST_JSON_DIR}/http_get_multi_trans.json 
         -r ${TEST_PCAP_DIR}/http_get_multi_trans.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_GET_LONG_COOKIE_TEST COMMAND ./${TEST_MAIN} ${TEST_JSON_DIR}/http_get_long_cookie.json 
         -r ${TEST_PCAP_DIR}/http_get_long_cookie.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_GET_ENCODED_URI_TEST COMMAND ./${TEST_MAIN} ${TEST_JSON_DIR}/http_get_encoded_uri.json 
         -r ${TEST_PCAP_DIR}/http_get_encoded_uri.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_RES_GZIP_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_res_gzip.json
         -r ${TEST_PCAP_DIR}/http_res_gzip.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_CHUNKED_RES_GZIP_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_chunked_res_gzip.json
         -r ${TEST_PCAP_DIR}/http_chunked_res_gzip.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_OVER_TCP_KEEPALIVE_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_over_tcp_keepalive.json
         -r ${TEST_PCAP_DIR}/http_over_tcp_keepalive.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_TUNNEL_FOR_POP3_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_tunnel_for_pop3.json 
         -r ${TEST_PCAP_DIR}/http_tunnel_for_pop3.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_TUNNEL_FOR_HTTP_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_tunnel_for_http.json 
         -r ${TEST_PCAP_DIR}/http_tunnel_for_http.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_OVER_PPPOE_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_over_pppoe.json
         -r ${TEST_PCAP_DIR}/http_over_pppoe.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_OVER_TLS_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_over_tls.json
         -r ${TEST_PCAP_DIR}/http_over_tls.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_NON_HTTP_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/non_http.json
         -r ${TEST_PCAP_DIR}/non_http.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_REQ_1BYTE_SLIDING_WINDOW_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_req_1byte_sliding_window.json
         -r ${TEST_PCAP_DIR}/http_req_1byte_sliding_window.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_RES_1BYTE_SLIDING_WINDOW_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_res_1byte_sliding_window.json
         -r ${TEST_PCAP_DIR}/http_res_1byte_sliding_window.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_NO_CONTENT_LENGTH_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_no_content_length.json
         -r ${TEST_PCAP_DIR}/http_no_content_length.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_POST_MULTIPART_FORM_DATA_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_post_multipart_form_data.json
         -r ${TEST_PCAP_DIR}/http_post_multipart_form_data.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_HEADERS_EXCEED_MAXIMUM_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_hdrs_exceed_maximum.json
         -r ${TEST_PCAP_DIR}/http_hdrs_exceed_maximum.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_GET_MALFORMED_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_get_malformed.json
         -r ${TEST_PCAP_DIR}/http_get_malformed.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_HEADER_VALUE_EMPTY_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_hdr_value_empty.json
         -r ${TEST_PCAP_DIR}/http_hdr_value_empty.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_UPGRADE_WEBSOCKET_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_upgrade_websocket.json
         -r ${TEST_PCAP_DIR}/http_upgrade_websocket.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_UPGRADE_HTTP2_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_upgrade_http2.json
         -r ${TEST_PCAP_DIR}/http_upgrade_http2.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_MULTI_PARSE_ERROR_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_multi_parse_error.json
         -r ${TEST_PCAP_DIR}/http_multi_parse_error.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_GET_REQ_PIPELINE_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_get_req_pipeline.json
         -r ${TEST_PCAP_DIR}/http_get_req_pipeline.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_TRANS_PIPELINE_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_trans_pipeline.json
         -r ${TEST_PCAP_DIR}/http_trans_pipeline.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_HEADER_TRUNCATED_IN_KV_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_hdr_truncated_in_kv.json
         -r ${TEST_PCAP_DIR}/http_hdr_truncated_in_kv.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME STELLAR_HTTP_HEADER_TRUNCATED_AFTER_KV_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_hdr_truncated_after_kv.json
         -r ${TEST_PCAP_DIR}/http_hdr_truncated_after_kv.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})

set_tests_properties(STELLAR_HTTP_GET_SINGLE_TRANS_TEST
                    STELLAR_HTTP_GET_MULTI_TRANS_TEST
                    STELLAR_HTTP_GET_LONG_COOKIE_TEST
                    STELLAR_HTTP_GET_ENCODED_URI_TEST
                    STELLAR_HTTP_RES_GZIP_TEST
                    STELLAR_HTTP_CHUNKED_RES_GZIP_TEST
                    STELLAR_HTTP_OVER_TCP_KEEPALIVE_TEST
                    STELLAR_HTTP_TUNNEL_FOR_POP3_TEST
                    STELLAR_HTTP_TUNNEL_FOR_HTTP_TEST
                    STELLAR_HTTP_OVER_PPPOE_TEST
                    STELLAR_HTTP_OVER_TLS_TEST
                    STELLAR_NON_HTTP_TEST
                    STELLAR_HTTP_REQ_1BYTE_SLIDING_WINDOW_TEST
                    STELLAR_HTTP_RES_1BYTE_SLIDING_WINDOW_TEST
                    STELLAR_HTTP_NO_CONTENT_LENGTH_TEST
                    STELLAR_HTTP_POST_MULTIPART_FORM_DATA_TEST
                    STELLAR_HTTP_HEADERS_EXCEED_MAXIMUM_TEST
                    STELLAR_HTTP_GET_MALFORMED_TEST
                    STELLAR_HTTP_MULTI_PARSE_ERROR_TEST
                    STELLAR_HTTP_HEADER_VALUE_EMPTY_TEST
                    STELLAR_HTTP_UPGRADE_WEBSOCKET_TEST
                    STELLAR_HTTP_UPGRADE_HTTP2_TEST
                    STELLAR_HTTP_GET_REQ_PIPELINE_TEST
                    STELLAR_HTTP_TRANS_PIPELINE_TEST
                    STELLAR_HTTP_HEADER_TRUNCATED_IN_KV_TEST
                    STELLAR_HTTP_HEADER_TRUNCATED_AFTER_KV_TEST
                    PROPERTIES FIXTURES_REQUIRED TestFixture)

add_test(NAME UPDATE_STATE_PLUG_ENTRY COMMAND bash -c "sed -i 's/name=.*/name=\\x22http_decoder_test_state_entry\\x22/' ${TEST_RUN_DIR}/etc/http/gtest_entry.toml")
add_test(NAME UPDATE_STATE_PLUG_TOPIC COMMAND bash -c "sed -i 's/topic=.*/topic=\\x22HTTP_DECODER_MESSAGE\\x22/' ${TEST_RUN_DIR}/etc/http/gtest_entry.toml")

set_tests_properties(UPDATE_STATE_PLUG_ENTRY UPDATE_STATE_PLUG_TOPIC
                     PROPERTIES FIXTURES_SETUP TestState)

add_test(NAME STELLAR_HTTP_MSG_TYPE_STATE_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_msg_type_state.json  
         -r ${TEST_PCAP_DIR}/http_post.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})
add_test(NAME STELLAR_HTTP_MSG_TYPE_STATE_C2S_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_msg_type_state_c2s.json 
        -r ${TEST_PCAP_DIR}/http_post_c2s.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})         
add_test(NAME STELLAR_HTTP_MSG_TYPE_STATE_S2C_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_msg_type_state_s2c.json 
        -r ${TEST_PCAP_DIR}/http_post_s2c.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})      
add_test(NAME STELLAR_HTTP_MSG_TYPE_STATE_PIPELINE_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_msg_type_state_pipeline.json 
        -r ${TEST_PCAP_DIR}/http_get_multi_trans.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})    
add_test(NAME STELLAR_HTTP_MSG_TYPE_STATE_SES_EXCEPTION_C2S_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_msg_type_state_exception_c2s.json
        -r ${TEST_PCAP_DIR}/http_session_exception_c2s.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})    
add_test(NAME STELLAR_HTTP_MSG_TYPE_STATE_SES_EXCEPTION_S2C_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_msg_type_state_exception_s2c.json
        -r ${TEST_PCAP_DIR}/http_session_exception_s2c.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})    
add_test(NAME STELLAR_HTTP_MSG_TYPE_STATE_TUNNEL_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_msg_type_state_tunnel.json
        -r ${TEST_PCAP_DIR}/http_tunnel_for_http.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})           
set_tests_properties(STELLAR_HTTP_MSG_TYPE_STATE_TEST
                    STELLAR_HTTP_MSG_TYPE_STATE_C2S_TEST
                    STELLAR_HTTP_MSG_TYPE_STATE_S2C_TEST
                    STELLAR_HTTP_MSG_TYPE_STATE_PIPELINE_TEST
                    STELLAR_HTTP_MSG_TYPE_STATE_SES_EXCEPTION_C2S_TEST
                    STELLAR_HTTP_MSG_TYPE_STATE_SES_EXCEPTION_S2C_TEST
                    STELLAR_HTTP_MSG_TYPE_STATE_TUNNEL_TEST
                    PROPERTIES FIXTURES_REQUIRED TestState)

add_test(NAME UPDATE_TUNNEL_PLUG_ENTRY COMMAND bash -c "sed -i 's/name=.*/name=\\x22http_decoder_tunnel_entry\\x22/' ${TEST_RUN_DIR}/etc/http/gtest_entry.toml")
add_test(NAME UPDATE_TUNNEL_PLUG_TOPIC COMMAND bash -c "sed -i 's/topic=.*/topic=\\x22HTTP_DECODER_TUNNEL_MESSAGE\\x22/' ${TEST_RUN_DIR}/etc/http/gtest_entry.toml")

set_tests_properties(UPDATE_TUNNEL_PLUG_ENTRY UPDATE_TUNNEL_PLUG_TOPIC
                     STELLAR_HTTP_DECODER_SO STELLAR_HTTP_DECODER_GTEST_SO
                     PROPERTIES FIXTURES_SETUP TestTunnel)     

add_test(NAME STELLAR_HTTP_MSG_TYPE_TUNNEL_POP3_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_inner_tunnel_for_pop3.json  
        -r ${TEST_PCAP_DIR}/http_tunnel_for_pop3.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})        
add_test(NAME STELLAR_HTTP_MSG_TYPE_TUNNEL_HTTP_TEST COMMAND  ./${TEST_MAIN} ${TEST_JSON_DIR}/http_inner_tunnel_for_http.json  
        -r ${TEST_PCAP_DIR}/http_tunnel_for_http.pcap  WORKING_DIRECTORY ${TEST_RUN_DIR})     

set_tests_properties(STELLAR_HTTP_MSG_TYPE_TUNNEL_POP3_TEST
                     STELLAR_HTTP_MSG_TYPE_TUNNEL_HTTP_TEST   
                     PROPERTIES FIXTURES_REQUIRED TestTunnel)                        