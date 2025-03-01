cmake_minimum_required(VERSION 3.15)
project(monitoring-daemon VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

# Set source files
set(SOURCES
    main.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${CURL_INCLUDE_DIRS})

# For Windows-specific code
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32)
endif()

# Installation rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)