cmake_minimum_required(VERSION 3.23)
project(webserver CXX)

set(CMAKE_CXX_STANDARD 20)

if (DEFINED PACKAGE_VERSION)
    add_compile_definitions(VERSION="${PACKAGE_VERSION}")
endif ()

if (DEFINED APPNAME)
    add_compile_definitions(APPNAME="${APPNAME}")
endif ()

add_compile_definitions(MG_ENABLE_OPENSSL=1)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")
    message("Enabled optimization compiler flags.")
else ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
    message("Disabled optimization flags for use in profiler.")
endif ()

file(GLOB resource_files CONFIGURE_DEPENDS LIST_DIRECTORIES false "resources/*")

# Creates C resources file from files in given directory
function(create_resources output)
    # Create empty output file
    file(WRITE ${output}
            "#ifndef WEBSERVER_RESOURCES_HPP\n"
            "#define WEBSERVER_RESOURCES_HPP\n"
            "\n"
            "#define LEN(name) name##_len\n"
            "#define RESOURCE(rc) reinterpret_cast<const char*>(rc)"
            "\n")
    # Iterate through input files
    foreach (bin ${resource_files})
        message("[RC] Generating ${bin} resource...")
        file(REAL_PATH ${bin} freal)
        file(SIZE ${freal} fsize)
        # Get short filename
        string(REGEX MATCH "([^/]+)$" filename ${bin})
        # Replace filename spaces & extension separator for C compatibility
        string(REGEX REPLACE "\\.| |-" "_" filename ${filename})
        # Read hex data from file
        file(READ ${bin} filedata HEX)
        if (NOT "${fsize}" STREQUAL "0")
            # Convert hex data for C compatibility
            string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," filedata ${filedata})
        endif ()
        # Append data to output file
        file(APPEND ${output}
                "\n"
                "#ifndef RESOURCE_${filename}\n"
                "#define RESOURCE_${filename}\n"
                " static constexpr const unsigned char ${filename}[] = {${filedata} 0};\n"
                " static constexpr const unsigned ${filename}_len = ${fsize};\n"
                "#endif\n"
                "\n")
    endforeach ()
    # Create empty output file
    file(APPEND ${output} "\n#endif //WEBSERVER_RESOURCES_HPP")
endfunction()

create_resources("resources.hpp")


message(" [ compiler ]  ${CMAKE_CXX_COMPILER}_${CMAKE_CXX_STANDARD} ${CMAKE_CXX_FLAGS}")

set(CMAKE_VERBOSE_MAKEFILE on)

add_executable(webserver
        sources/main.cpp
        sources/server.cpp
        sources/config.cpp
        sources/tools.cpp)

target_link_libraries(webserver pthread ssl crypto fineftp-server curl)
