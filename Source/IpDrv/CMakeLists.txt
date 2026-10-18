project(IpDrv CXX)

set(SRC_FILES
  "Src/TcpLink.cpp"
  "Src/UdpLink.cpp"
  "Src/IpDrv.cpp"
  "Src/UnSocket.cpp"
  "Src/InternetLink.cpp"
  "Src/TcpNetDriver.cpp"
)
# UMasterServerCommandlet/UUpdateServerCommandlet (master-server tooling) are not built: HP2 has no server
# browser or master server. No IpDrv.u ships either: TcpNetDriver needs no script class.

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
  # On the web AUdpLink calls the synthetic LAN in WebRTCDrv (WebRTCLan.h). Header only: the calls
  # resolve at the final --whole-archive link, so IpDrv does not link WebRTCDrv and there is no cycle.
  ${CMAKE_CURRENT_SOURCE_DIR}/../WebRTCDrv
)

target_link_libraries(${PROJECT_NAME} Engine Core)

if(TARGET_IS_WINDOWS)
  target_link_libraries(${PROJECT_NAME} ws2_32)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE IPDRV_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})
