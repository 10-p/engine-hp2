project(Editor CXX)

# HP2 build 433 Editor, stripped the way the template strips UT99's: no Win32 GUI hook (EdHook.cpp is
# replaced by NullEdHook.cpp), no TrueType import (TTFontImport.cpp needs GDI), no PS2 commandlets
# (UPS2ConvertCommandlet, UPSX2MusicCommandlet). It links only Engine+Core and exists because UE1 content
# references Editor classes at runtime; it also carries `ucc make`.
set(SRC_FILES
  "Src/CoolBsp.cpp"
  "Src/Editor.cpp"
  "Src/UBatchExportCommandlet.cpp"
  "Src/UBrushBuilder.cpp"
  "Src/UConformCommandlet.cpp"
  "Src/UMakeCommandlet.cpp"
  "Src/UMasterCommandlet.cpp"
  "Src/UMergeDXTCommandlet.cpp"
  "Src/UPkgCommandlet.cpp"
  "Src/UnBsp.cpp"
  "Src/UnEdAct.cpp"
  "Src/UnEdCam.cpp"
  "Src/UnEdClick.cpp"
  "Src/UnEdCnst.cpp"
  "Src/UnEdCsg.cpp"
  "Src/UnEdExp.cpp"
  "Src/UnEdFact.cpp"
  "Src/UnEditor.cpp"
  "Src/UnEdRend.cpp"
  "Src/UnEdSrv.cpp"
  "Src/UnEdTran.cpp"
  "Src/UnMatinee.cpp"
  "Src/UnMeshEd.cpp"
  "Src/UnMeshLP.cpp"
  "Src/UnParams.cpp"
  "Src/UnScrCom.cpp"
  "Src/UnShadow.cpp"
  "Src/UnSyntax.cpp"
  "Src/UnTopics.cpp"
  "Src/UnVisi.cpp"
  "Src/NullEdHook.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_link_libraries(${PROJECT_NAME} Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE EDITOR_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})
