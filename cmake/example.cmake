add_definitions("-DFDB_IDL_EXAMPLE_H=<idl-gen/common.base.Example.pb.h>")

include_directories(
    ${PACKAGE_SOURCE_ROOT}/example
)

link_directories(${PACKAGE_SOURCE_ROOT}/../protobuf/src/.libs)
#link_libraries(protobuf-lite)
#link_libraries(protobuf)

#add_executable(fdbobjtest
#    ${PACKAGE_SOURCE_ROOT}/example/client_server_object.cpp
#    ${IDL_GEN_ROOT}/idl-gen/common.base.Example.pb.cc
#)

add_executable(fdbjobtest
    ${PACKAGE_SOURCE_ROOT}/example/job/job_test.cpp
)


add_executable(fdbclienttest
    ${PACKAGE_SOURCE_ROOT}/example/client-server/fdb_test_client.cpp
   ${PACKAGE_SOURCE_ROOT}/example/idl-gen/common.base.Example.pb.cc
)
target_link_libraries(fdbclienttest protobuf-lite protobuf)

add_executable(fdbservertest
    ${PACKAGE_SOURCE_ROOT}/example/client-server/fdb_test_server.cpp
    ${PACKAGE_SOURCE_ROOT}/example/idl-gen/common.base.Example.pb.cc
)
target_link_libraries(fdbservertest protobuf-lite protobuf)



#add_executable(fdbntfcentertest
#    ${PACKAGE_SOURCE_ROOT}/example/fdb_test_server.cpp
#    ${IDL_GEN_ROOT}/idl-gen/common.base.Example.pb.cc
#)

#install(TARGETS fdbobjtest fdbjobtest fdbclienttest fdbservertest fdbntfcentertest RUNTIME DESTINATION usr/bin)
