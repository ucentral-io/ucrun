project(urun C)
cmake_minimum_required(VERSION 2.8.12)

set(SAMP urun)
set(SRCS main.c ucode.c ubus.c)

add_executable(${SAMP} ${SRCS})
target_link_libraries(${SAMP} ubox)
target_link_libraries(${SAMP} blobmsg_json)
target_link_libraries(${SAMP} ucode)
target_link_libraries(${SAMP} ubus)
