project(futurerestore)
add_executable(futurerestore
        main.cpp
        futurerestore.cpp)
target_include_directories(futurerestore PRIVATE
        "${CMAKE_SOURCE_DIR}/dep_root/include"
        "${CMAKE_SOURCE_DIR}/external/idevicerestore/src"
        "${CMAKE_SOURCE_DIR}/external/tsschecker/external/jssy/jssy"
        "${CMAKE_SOURCE_DIR}/external/tsschecker/tsschecker")
target_link_directories(futurerestore PRIVATE
        "${CMAKE_SOURCE_DIR}/dep_root/lib"
        "${CMAKE_SOURCE_DIR}/dep_root/lib/xpwn"
        )
target_link_libraries(futurerestore PRIVATE
        z
        zip
        png16
        crypto
        ssl
        "-lgeneral"
        plist-2.0
        fragmentzip
        img4tool
        common
        xpwn
        insn
        offsetfinder64
        ipatcher
        tsschecker
        idevicerestore
        jssy
        curl
        imobiledevice-glue-1.0
        imobiledevice-1.0
        irecovery-1.0
        usbmuxd-2.0
        pthread)
if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(futurerestore PRIVATE
                compression
                "-framework CoreFoundation"
                "-framework IOKit")
else()
    target_link_libraries(futurerestore PRIVATE
            usb-1.0
            dl
            udev
            lzfse)
endif()
execute_process(COMMAND git rev-list --count HEAD WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE VERSION_COMMIT_COUNT ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE VERSION_COMMIT_SHA ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND cat version.txt WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE VERSION_RELEASE ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
add_definitions(
        -DHAVE_LIBIPATCHER=1
        -DVERSION_COMMIT_COUNT="${VERSION_COMMIT_COUNT}"
        -DVERSION_COMMIT_SHA="${VERSION_COMMIT_SHA}"
        -DVERSION_RELEASE="${VERSION_RELEASE}")
