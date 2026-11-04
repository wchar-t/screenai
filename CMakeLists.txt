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

add_library(screenai_sdk SHARED
	src/api/screenai_api.cpp
	src/backend/chrome_screen_ai/annotation_decoder.cpp
	src/backend/chrome_screen_ai/chrome_backend.cpp
	src/backend/chrome_screen_ai/model_files.cpp
	src/core/recognizer.cpp
	src/core/runtime_config.cpp
	src/image/wic_decoder.cpp
)

target_compile_definitions(screenai_sdk PRIVATE SCREENAI_BUILD)
target_include_directories(screenai_sdk
	PUBLIC include
	PRIVATE src
)
target_link_libraries(screenai_sdk PRIVATE ole32 windowscodecs)
set_target_properties(screenai_sdk PROPERTIES OUTPUT_NAME screenai)
if(MSVC)
	target_compile_options(screenai_sdk PRIVATE /W4 /permissive-)
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
