# ArtifactDeps.cmake — the provenance of the fetched libraries, recorded in meta.json.
#
# Every third-party repository this build fetches is declared as <repo>@<branch|tag>, never as a commit:
# the project's forks (10-p/*) and own repositories at their branch tip, an external upstream at its
# release tag. meta.json's `toolchain.<lib>` names that repo and ref, fed from the same cache variables
# FetchContent_Declare reads. A local checkout override (FETCHCONTENT_SOURCE_DIR_<NAME>, or an
# engine-specific <LIB>_SOURCE_DIR) is recorded as `local:<path>`: such an artifact is not reproducible
# from meta.json alone.
#
#   ufront_artifact_dep(<list-var> <label> <fetchcontent-name> <repo> <ref> [<local-source-dir>])
#     appends "--dep-<label>=<repo>@<ref>" (or "--dep-<label>=local:<dir>") to <list-var>, the argument
#     form Game/cmake/emit-artifact-meta.mjs takes.
#
# FetchContent re-fetches a branch ref on every configure (FETCHCONTENT_UPDATES_DISCONNECTED is OFF) and
# never on a plain `cmake --build`: a build is at each branch's tip only after a configure.
function(ufront_artifact_dep out label fcname repo ref)
  string(TOUPPER "${fcname}" _u)
  set(_local "${FETCHCONTENT_SOURCE_DIR_${_u}}")
  if(NOT _local AND ARGC GREATER 5)
    set(_local "${ARGV5}")
  endif()
  if(_local)
    set(_spec "local:${_local}")
  else()
    set(_spec "${repo}@${ref}")
  endif()
  list(APPEND ${out} "--dep-${label}=${_spec}")
  set(${out} "${${out}}" PARENT_SCOPE)
endfunction()
