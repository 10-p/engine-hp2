project(Engine CXX)

# HP2 build 433 Engine: the Engine.dsp list minus winstuff.cpp (Win32 GDI helpers behind UnEngineWin.h)
# and minus the MPeg wrappers it listed but never referenced (Thirdparty/MPeg). Everything else — the
# 433 additions (skeletal meshes, particles, gestures, replay, interpolation, downloads, demo pending
# level, the native-function table) included — compiles on every target.
set(SRC_FILES
  "Src/AStatLog.cpp"
  "Src/Engine.cpp"
  "Src/palette.cpp"
  "Src/ULodMesh.cpp"
  "Src/UnActCol.cpp"
  "Src/UnActor.cpp"
  "Src/UnAudio.cpp"
  "Src/UnBunch.cpp"
  "Src/UnCamera.cpp"
  "Src/UnCamMgr.cpp"
  "Src/UnCanvas.cpp"
  "Src/UnChan.cpp"
  "Src/UnCon.cpp"
  "Src/UnConn.cpp"
  "Src/UnDemoPenLev.cpp"
  "Src/UnDemoRec.cpp"
  "Src/UnDownload.cpp"
  "Src/UnDynBsp.cpp"
  "Src/UnEngine.cpp"
  "Src/UnEngineNative.cpp"
  "Src/UnFont.cpp"
  "Src/UnFPoly.cpp"
  "Src/UnGame.cpp"
  "Src/UnGesture.cpp"
  "Src/UnIn.cpp"
  "Src/UnLevAct.cpp"
  "Src/UnLevel.cpp"
  "Src/UnLevTic.cpp"
  "Src/UnMesh.cpp"
  "Src/UnModel.cpp"
  "Src/UnMover.cpp"
  "Src/UnNetDrv.cpp"
  "Src/UnParams.cpp"
  "Src/UnParticle.cpp"
  "Src/UnParticleFX.cpp"
  "Src/UnParticleList.cpp"
  "Src/UnPath.cpp"
  "Src/UnPawn.cpp"
  "Src/UnPenLev.cpp"
  "Src/UnPhysic.cpp"
  "Src/UnPlayer.cpp"
  "Src/UnPrim.cpp"
  "Src/UnReach.cpp"
  "Src/UnRenderIterator.cpp"
  "Src/UnReplay.cpp"
  "Src/UnRoute.cpp"
  "Src/UnScript.cpp"
  "Src/UnScrTex.cpp"
  "Src/UnSkeletalMesh.cpp"
  "Src/UnTex.cpp"
  "Src/UnTrace.cpp"
  "Src/UnURL.cpp"
  "Src/UnWind.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_link_libraries(${PROJECT_NAME} Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE ENGINE_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})
