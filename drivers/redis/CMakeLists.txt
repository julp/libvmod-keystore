cmake_minimum_required(VERSION 2.8.3)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR} "../..")
find_package(Hiredis REQUIRED)

if(PROJECT_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(VarnishMOD)

    add_definitions(-DREDIS_SHARED_DRIVER=1)
    declare_vmod(
        INSTALL
        NAME keystore_redis
        VCC src/vmod_keystore_redis.vcc
        SOURCES src/vmod_keystore_redis.c
        ADDITIONNAL_LIBRARIES ${HIREDIS_LIBRARIES} "${VARNISHAPI_VMODDIR}/libvmod_keystore.so"
        ADDITIONNAL_INCLUDE_DIRECTORIES ${HIREDIS_INCLUDE_DIRS} "../../src"
    )
else(PROJECT_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    declare_driver(
        NAME "redis"
        SOURCES src/vmod_keystore_redis.c
        ADDITIONNAL_LIBRARIES ${HIREDIS_LIBRARIES}
        ADDITIONNAL_INCLUDE_DIRECTORIES ${HIREDIS_INCLUDE_DIRS}
    )
endif(PROJECT_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
