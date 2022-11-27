cmake_minimum_required(VERSION 3.16)

project("Silvanus")

set(CMAKE_CXX_STANDARD 17)
include("cmake/SystemTarget.cmake")

# Detect if building on raspberry pi
find_file(BCM_HOST_PATH NAMES "bcm_host.h" PATHS /opt/vc/include/)
message("BCM host header: ${BCM_HOST_PATH}")

# Help macOS builds find OpenSSL (requires brew OpenSSL install)
if (APPLE)
    set (OPENSSL_ROOT_DIR /opt/homebrew/opt/openssl@3)
endif()
find_package(OpenSSL REQUIRED)

# httplib - Cmake, header-only
add_subdirectory("deps/cpp-httplib")
system_target(httplib) # Silence annoying warnings

# fmt - Cmake, header-only
add_subdirectory("deps/fmt")

# sigslot - Cmake, header-only
add_subdirectory("deps/sigslot")

# json - Cmake, header-only
set(JSON_BuildTests OFF)
add_subdirectory("deps/json")
system_target(nlohmann_json) # Silence annoying warnings

add_executable( ${PROJECT_NAME} 
                    src/ConfigService.cpp
                    src/HttpService.cpp
                    src/InputButton.cpp
                    src/main.cpp )

# Set some compiler flags used for pi-only features
if (BCM_HOST_PATH)
    target_compile_definitions(${PROJECT_NAME} PUBLIC "PI_HOST")
    target_compile_definitions(${PROJECT_NAME} PUBLIC "LINUX_HID_CONTROLLER_SUPPORT")
    target_include_directories(${PROJECT_NAME} PRIVATE "/opt/vc/include")
    target_link_directories(${PROJECT_NAME} PRIVATE "/opt/vc/lib")
    target_link_libraries(${PROJECT_NAME} bcm_host)
else()
    # include PC-specific libs here if desired
endif()

# Link the c++ filesystem API under GCC
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(${PROJECT_NAME} stdc++fs)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} fmt nlohmann_json httplib Pal::Sigslot OpenSSL::SSL OpenSSL::Crypto)

add_custom_target(copy_resources ALL)

add_custom_command(TARGET copy_resources POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${PROJECT_SOURCE_DIR}/resources
                    ${CMAKE_CURRENT_BINARY_DIR}/resources)
