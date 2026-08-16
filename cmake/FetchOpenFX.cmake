include(FetchContent)

if(EMBR_FETCH_OPENFX)
  FetchContent_Declare(
    openfx
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openfx.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
  )
  FetchContent_GetProperties(openfx)
  if(NOT openfx_POPULATED)
    FetchContent_Populate(openfx)
  endif()
  set(OPENFX_ROOT "${openfx_SOURCE_DIR}" CACHE PATH "OpenFX SDK root")
else()
  if(NOT OPENFX_ROOT)
    message(FATAL_ERROR "Set OPENFX_ROOT or enable EMBR_FETCH_OPENFX")
  endif()
endif()

set(OPENFX_INCLUDE_DIR "${OPENFX_ROOT}/include")
set(OPENFX_SUPPORT_INCLUDE_DIR "${OPENFX_ROOT}/Support/include")
set(OPENFX_SUPPORT_LIBRARY_DIR "${OPENFX_ROOT}/Support/Library")

set(OPENFX_SUPPORT_SOURCES
  "${OPENFX_SUPPORT_LIBRARY_DIR}/ofxsCore.cpp"
  "${OPENFX_SUPPORT_LIBRARY_DIR}/ofxsImageEffect.cpp"
  "${OPENFX_SUPPORT_LIBRARY_DIR}/ofxsInteract.cpp"
  "${OPENFX_SUPPORT_LIBRARY_DIR}/ofxsLog.cpp"
  "${OPENFX_SUPPORT_LIBRARY_DIR}/ofxsMultiThread.cpp"
  "${OPENFX_SUPPORT_LIBRARY_DIR}/ofxsParams.cpp"
  "${OPENFX_SUPPORT_LIBRARY_DIR}/ofxsProperty.cpp"
  "${OPENFX_SUPPORT_LIBRARY_DIR}/ofxsPropertyValidation.cpp"
)
