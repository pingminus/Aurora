# PowerShell returns native backslashes. CEF's generated target properties
# require CMake paths, otherwise a Windows user path is parsed as a \U escape.
file(TO_CMAKE_PATH "${CEF_ROOT}" CEF_ROOT)
if(NOT IS_DIRECTORY "${CEF_ROOT}/include")
  message(FATAL_ERROR "Set CEF_ROOT to the extracted pinned CEF SDK. Run tools/fetch-cef.ps1.")
endif()
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
set(USE_SANDBOX ON CACHE BOOL "Required Chromium sandbox" FORCE)
find_package(CEF REQUIRED)
add_subdirectory("${CEF_LIBCEF_DLL_WRAPPER_PATH}" "${CMAKE_BINARY_DIR}/libcef_dll_wrapper")
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
enable_language(RC)
add_library(opengod SHARED opengod.rc main.cpp resources.cpp cve_service.cpp window_frame.cpp tab_audio.cpp ../audio/tab_audio_output.cpp ../terminal/session.cpp)
# Delay-load only OpenGod's imported platform DLLs (checked with dumpbin
# /DEPENDENTS). Audio/graphics must not load before sandbox subprocess entry.
list(FILTER CEF_SHARED_LINKER_FLAGS EXCLUDE REGEX "^/DELAYLOAD:")
list(APPEND CEF_SHARED_LINKER_FLAGS /DELAYLOAD:libcef.dll /DELAYLOAD:user32.dll /DELAYLOAD:gdi32.dll /DELAYLOAD:ole32.dll /DELAYLOAD:comctl32.dll /DELAYLOAD:advapi32.dll)
SET_LIBRARY_TARGET_PROPERTIES(opengod)
target_compile_features(opengod PRIVATE cxx_std_20)
target_include_directories(opengod PRIVATE "${CEF_ROOT}" "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(opengod PRIVATE opengod_core libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS} xaudio2 ole32 comctl32 advapi32)
target_compile_options(opengod PRIVATE /W4 /utf-8)
target_compile_definitions(opengod PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_target_properties(opengod PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_property(TARGET opengod APPEND PROPERTY LINK_DEPENDS
  "${PROJECT_SOURCE_DIR}/ui/icons/opengod.ico"
  "${PROJECT_SOURCE_DIR}/tools/set-package-icon.ps1")
add_custom_command(TARGET opengod POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CEF_ROOT}/Release" "$<TARGET_FILE_DIR:opengod>"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CEF_ROOT}/Resources" "$<TARGET_FILE_DIR:opengod>"
  COMMAND ${CMAKE_COMMAND} -E copy "${CEF_ROOT}/Release/bootstrap.exe" "$<TARGET_FILE_DIR:opengod>/opengod.exe"
  COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${PROJECT_SOURCE_DIR}/tools/set-package-icon.ps1" -Executable "$<TARGET_FILE_DIR:opengod>/opengod.exe" -Icon "${PROJECT_SOURCE_DIR}/ui/icons/opengod.ico"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/ui/dist" "$<TARGET_FILE_DIR:opengod>/ui"
  COMMAND ${CMAKE_COMMAND} -E copy "${CEF_ROOT}/LICENSE.txt" "$<TARGET_FILE_DIR:opengod>/CEF-LICENSE.txt")
