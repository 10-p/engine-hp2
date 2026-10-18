project(UCC CXX)

# ucc — the command-line tool (UnrealScript `make` and the other commandlets). A headless console executable
# (no viewport, no SDL, no renderer): UCC.cpp runs the UCommandlet named on the command line. Native only.
set(SRC_FILES
  "Src/UCC.cpp"
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

# The binary is `ucc-bin`, the classic Linux UE1 name.
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ucc-bin)

target_include_directories(${PROJECT_NAME}
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

if(BUILD_STATIC)
  # A static build (the `native` preset: UNREAL_STATIC) has no package files to load: appGetDllHandle answers
  # from the in-binary export registry (Core/UnStaticExports), which holds a package only if its objects are in
  # this binary. As plain archives Engine and Core contribute only the objects UCC.cpp references, and no
  # commandlet outside Core is found. So link the package set the way the game target does
  # (Source/Game/CMakeLists.txt), under --whole-archive so each package's GLoaded<Pkg> export and
  # IMPLEMENT_CLASS registrants are kept, minus what a headless tool never opens: the viewport (NSDLDrv/SDL2),
  # the render devices and the audio subsystems.
  #   Render  -- the software scene renderer; no external libraries, kept so client-flagged commandlets can load it.
  #   IpDrv   -- TcpNetDriver (the server commandlet's listen socket), UdpLink/TcpLink.
  #   Fire    -- procedural textures referenced by the maps.
  #   Editor  -- carries `ucc make`; content can reference Editor classes.
  set(UCC_LINK_PACKAGES Core Engine Render IpDrv Fire)
  if(BUILD_EDITOR)
    list(APPEND UCC_LINK_PACKAGES Editor)
  endif()
  target_link_libraries(${PROJECT_NAME} -Wl,--whole-archive ${UCC_LINK_PACKAGES} -Wl,--no-whole-archive)
else()
  # Shared build: Engine + Core only. Editor (make), IpDrv (server) and the game packages load at runtime.
  target_link_libraries(${PROJECT_NAME} Engine Core)
endif()

# UCC.cpp is retail-era code: its file-manager #if tests the legacy uppercase __LINUX__. Define it so ucc
# picks FFileManagerLinux + FMallocAnsi.
target_compile_definitions(${PROJECT_NAME} PRIVATE UPACKAGE_NAME=${PROJECT_NAME} __LINUX__)
