cmake_minimum_required(VERSION 3.25)

cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)

project(ScreenAI LANGUAGES CXX RC)

if(NOT WIN32)
	message(FATAL_ERROR "ScreenAI currently supports Windows only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(
	SCREENAI_RUNTIME_BUNDLE
	"${CMAKE_CURRENT_SOURCE_DIR}/runtime/screenai-runtime-153.5.sai"
	CACHE FILEPATH
	"ScreenAI runtime package embedded into screenai.dll"
)

get_filename_component(
	SCREENAI_RUNTIME_BUNDLE
	"${SCREENAI_RUNTIME_BUNDLE}"
	ABSOLUTE
	BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(NOT EXISTS "${SCREENAI_RUNTIME_BUNDLE}")
	message(FATAL_ERROR "ScreenAI runtime bundle not found: ${SCREENAI_RUNTIME_BUNDLE}")
endif()

file(TO_CMAKE_PATH "${SCREENAI_RUNTIME_BUNDLE}" SCREENAI_RUNTIME_BUNDLE_RC)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
	src/resources/screenai_runtime.rc.in
	"${CMAKE_CURRENT_BINARY_DIR}/generated/screenai_runtime.rc"
	@ONLY
)

add_library(screenai_sdk SHARED
	"${CMAKE_CURRENT_BINARY_DIR}/generated/screenai_runtime.rc"
	src/api/screenai_api.cpp
	src/backend/chrome_screen_ai/annotation_decoder.cpp
	src/backend/chrome_screen_ai/chrome_backend.cpp
	src/backend/chrome_screen_ai/model_files.cpp
	src/backend/chrome_screen_ai/runtime_package.cpp
	src/core/recognizer.cpp
	src/core/runtime_config.cpp
	src/image/wic_decoder.cpp
)

target_compile_definitions(screenai_sdk PRIVATE SCREENAI_BUILD)
target_include_directories(screenai_sdk
	PUBLIC include
	PRIVATE src
)
target_link_libraries(screenai_sdk PRIVATE ole32 windowscodecs bcrypt)
set_target_properties(screenai_sdk PROPERTIES OUTPUT_NAME screenai)
if(MSVC)
	target_compile_options(screenai_sdk PRIVATE
		$<$<COMPILE_LANGUAGE:CXX>:/W4>
		$<$<COMPILE_LANGUAGE:CXX>:/permissive->
	)
endif()

add_executable(screenai src/cli/main.cpp)
target_link_libraries(screenai PRIVATE screenai_sdk)
if(MSVC)
	target_compile_options(screenai PRIVATE /W4 /permissive-)
endif()

include(CTest)
if(BUILD_TESTING)
	add_executable(annotation_decoder_test
		tests/annotation_decoder_test.cpp
		src/backend/chrome_screen_ai/annotation_decoder.cpp
	)
	target_include_directories(annotation_decoder_test PRIVATE src)
	if(MSVC)
		target_compile_options(annotation_decoder_test PRIVATE /W4 /permissive-)
	endif()
	add_test(NAME annotation_decoder_test COMMAND annotation_decoder_test)
endif()

install(TARGETS screenai_sdk screenai
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION bin
	ARCHIVE DESTINATION lib
)
install(DIRECTORY include/screenai DESTINATION include)
install(DIRECTORY python/screenai DESTINATION python)
