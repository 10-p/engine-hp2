project(WebRTCDrv)

# WebRTCDrv: the UE1 WebRTC net driver (Emscripten/browser networking). The
# transport is the glue's ue_net_* imports (vendor/glue/ue_abi.h, baked net-shim.js), not BSD sockets.
set(SRC_FILES
  "WebRTCDrv.cpp"
  # The synthetic LAN: answers UE1 LAN discovery on IpDrv's AUdpLink from the WebRTC peer list.
  # AUdpLink calls into it (one way), so this package's root is a PUBLIC include dir below.
  "WebRTCLan.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
  ${CMAKE_SOURCE_DIR}/../vendor/glue
)

target_link_libraries(${PROJECT_NAME} Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE WEBRTCDRV_EXPORTS UPACKAGE_NAME=${PROJECT_NAME}
  "WEBRTC_PEER_PREFIX=\"${HP2_TARGET_ID}\"")
