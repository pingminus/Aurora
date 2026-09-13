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
add_library(aurora SHARED main.cpp resources.cpp cve_service.cpp window_frame.cpp tab_audio.cpp ../audio/tab_audio_output.cpp ../terminal/session.cpp)
# Delay-load only Aurora's imported platform DLLs (checked with dumpbin
# /DEPENDENTS). Audio/graphics must not load before sandbox subprocess entry.
list(FILTER CEF_SHARED_LINKER_FLAGS EXCLUDE REGEX "^/DELAYLOAD:")
list(APPEND CEF_SHARED_LINKER_FLAGS /DELAYLOAD:libcef.dll /DELAYLOAD:user32.dll /DELAYLOAD:gdi32.dll /DELAYLOAD:ole32.dll /DELAYLOAD:comctl32.dll /DELAYLOAD:advapi32.dll)
SET_LIBRARY_TARGET_PROPERTIES(aurora)
target_compile_features(aurora PRIVATE cxx_std_20)
target_include_directories(aurora PRIVATE "${CEF_ROOT}" "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(aurora PRIVATE aurora_core libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS} xaudio2 ole32 comctl32 advapi32)
target_compile_options(aurora PRIVATE /W4 /utf-8)
target_compile_definitions(aurora PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_target_properties(aurora PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET aurora POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CEF_ROOT}/Release" "$<TARGET_FILE_DIR:aurora>"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CEF_ROOT}/Resources" "$<TARGET_FILE_DIR:aurora>"
  COMMAND ${CMAKE_COMMAND} -E copy "${CEF_ROOT}/Release/bootstrap.exe" "$<TARGET_FILE_DIR:aurora>/aurora.exe"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/ui/dist" "$<TARGET_FILE_DIR:aurora>/ui"
  COMMAND ${CMAKE_COMMAND} -E copy "${CEF_ROOT}/LICENSE.txt" "$<TARGET_FILE_DIR:aurora>/CEF-LICENSE.txt")
