cmake_minimum_required(VERSION 3.16)

project("Silvanus")

set(CMAKE_CXX_STANDARD 17)

add_executable( ${PROJECT_NAME} 
                    src/I2CDevice.cpp
                    src/Adafruit_SHT31.cpp
                    src/ConfigService.cpp
                    src/HttpService.cpp
                    src/Silvanus.cpp
                    src/main.cpp )

# A little bit of stuff to make the project build on PCs
find_file(BCM_HOST_PATH NAMES "bcm_host.h")
message("BCM host header: ${BCM_HOST_PATH}")
if (APPLE)
    set (OPENSSL_ROOT_DIR /opt/homebrew/opt/openssl@3)
endif()
if (BCM_HOST_PATH)
    target_compile_definitions(${PROJECT_NAME} PUBLIC "PI_HOST")
endif()

find_package(OpenSSL REQUIRED)

# httplib - Cmake, header-only
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE deps/cpp-httplib)

# fmt - Cmake, header-only
target_compile_definitions(${PROJECT_NAME} PUBLIC "FMT_HEADER_ONLY")
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE deps/fmt/include)

# sigslot - Cmake, header-only
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE deps/sigslot/include)

# json - Cmake, header-only
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE deps/json/include)

target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} OpenSSL::SSL OpenSSL::Crypto)
if (BCM_HOST_PATH)
  target_link_libraries(${PROJECT_NAME} stdc++fs bcm_host pthread)
endif()

add_custom_target(copy_resources ALL)

add_custom_command(TARGET copy_resources POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${PROJECT_SOURCE_DIR}/resources
                    ${CMAKE_CURRENT_BINARY_DIR}/resources)
