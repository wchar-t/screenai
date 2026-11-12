cmake_minimum_required(VERSION 3.25)

cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)

project(ScreenAI LANGUAGES CXX)

if(NOT WIN32)
	message(FATAL_ERROR "ScreenAI currently supports Windows only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SCREENAI_RUNTIME_VERSION "153.7")
set(
	SCREENAI_RUNTIME_PACKAGE_NAME
	"screenai-runtime-${SCREENAI_RUNTIME_VERSION}.sai"
)
set(
	SCREENAI_RUNTIME_PACKAGE
	"${CMAKE_CURRENT_SOURCE_DIR}/runtime/${SCREENAI_RUNTIME_PACKAGE_NAME}"
)

if(NOT EXISTS "${SCREENAI_RUNTIME_PACKAGE}")
	message(FATAL_ERROR
		"Required runtime package not found: ${SCREENAI_RUNTIME_PACKAGE}. "
		"Create it with: py tools/pack_runtime.py <runtime-source-directory>"
	)
endif()

add_library(screenai_sdk SHARED
	src/api/screenai_api.cpp
	src/api/worker_process.cpp
	src/backend/chrome_screen_ai/annotation_decoder.cpp
	src/backend/chrome_screen_ai/chrome_backend.cpp
	src/backend/chrome_screen_ai/model_files.cpp
	src/backend/chrome_screen_ai/runtime_package.cpp
	src/core/recognizer.cpp
	src/image/wic_decoder.cpp
)

set_property(TARGET screenai_sdk APPEND PROPERTY LINK_DEPENDS "${SCREENAI_RUNTIME_PACKAGE}")

target_compile_definitions(screenai_sdk PRIVATE
	SCREENAI_BUILD
	SCREENAI_RUNTIME_VERSION=\"${SCREENAI_RUNTIME_VERSION}\"
)
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

add_custom_command(TARGET screenai_sdk POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:screenai_sdk>/runtime"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${SCREENAI_RUNTIME_PACKAGE}"
		"$<TARGET_FILE_DIR:screenai_sdk>/runtime/${SCREENAI_RUNTIME_PACKAGE_NAME}"
	COMMENT "Copying the required ScreenAI runtime package"
	VERBATIM
)

add_executable(screenai
	src/cli/main.cpp
)
target_link_libraries(screenai PRIVATE screenai_sdk)
if(MSVC)
	target_compile_options(screenai PRIVATE /W4 /permissive-)
endif()

add_executable(screenai_worker
	src/worker/main.cpp
	src/worker/worker_host.cpp
)
target_include_directories(screenai_worker PRIVATE src)
target_link_libraries(screenai_worker PRIVATE screenai_sdk)
if(MSVC)
	target_compile_options(screenai_worker PRIVATE /W4 /permissive-)
endif()

include(CTest)
if(BUILD_TESTING)
	add_executable(annotation_decoder_test
		tests/annotation_decoder_test.cpp
		src/backend/chrome_screen_ai/annotation_decoder.cpp
	)
	target_include_directories(annotation_decoder_test PRIVATE src include)
	if(MSVC)
		target_compile_options(annotation_decoder_test PRIVATE /W4 /permissive-)
	endif()
	add_test(NAME annotation_decoder_test COMMAND annotation_decoder_test)
endif()

install(TARGETS screenai_sdk screenai screenai_worker
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION bin
	ARCHIVE DESTINATION lib
)
install(FILES "${SCREENAI_RUNTIME_PACKAGE}" DESTINATION bin/runtime)
install(DIRECTORY include/screenai DESTINATION include)
install(DIRECTORY python/screenai DESTINATION python)
