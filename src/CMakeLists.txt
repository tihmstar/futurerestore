project(futurerestore)
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
add_executable(futurerestore
        main.cpp
        futurerestore.cpp)
target_include_directories(futurerestore PRIVATE
        "${CMAKE_SOURCE_DIR}/external/idevicerestore/src"
        "${CMAKE_SOURCE_DIR}/external/tsschecker/external/jssy/jssy"
        "${CMAKE_SOURCE_DIR}/external/tsschecker/tsschecker")
set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS}")
if(NOT DEFINED NO_PKGCFG AND NOT "$ENV{NO_PKGCFG}" MATCHES "1")
    pkg_check_modules(DEPS REQUIRED
            libzip
            zlib
            libpng16
            libcrypto
            libssl
            libplist-2.0
            libimobiledevice-glue-1.0
            libimobiledevice-1.0
            libirecovery-1.0
            libusbmuxd-2.0
            libgeneral
            libcurl
            libfragmentzip
            libimg4tool
            libinsn
            liboffsetfinder64
            libipatcher)
    target_include_directories(futurerestore PRIVATE "${DEPS_INCLUDE_DIRS}")
    target_link_directories(futurerestore PRIVATE "${DEPS_LIBRARY_DIRS}")
    target_link_libraries(futurerestore PRIVATE "${DEPS_LIBRARIES}" tsschecker idevicerestore)
else()
    target_include_directories(futurerestore PRIVATE
            "${CMAKE_SOURCE_DIR}/dep_root/include")
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
    endif()
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
if(NOT DEFINED VERSION_COMMIT_COUNT)
    execute_process(COMMAND git rev-list --count HEAD WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE VERSION_COMMIT_COUNT ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
if(NOT DEFINED VERSION_COMMIT_SHA)
    execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE VERSION_COMMIT_SHA ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
execute_process(COMMAND cat version.txt WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE VERSION_RELEASE ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
add_definitions(
        -DHAVE_LIBIPATCHER=1
        -DVERSION_COMMIT_COUNT="${VERSION_COMMIT_COUNT}"
        -DVERSION_COMMIT_SHA="${VERSION_COMMIT_SHA}"
        -DVERSION_RELEASE="${VERSION_RELEASE}"
        -DPACKAGE_NAME="futurerestore"
        -DPACKAGE_VERSION="${VERSION_RELEASE} Build: ${VERSION_COMMIT_COUNT}\(${VERSION_COMMIT_SHA}\)")
if(DEFINED DESTDIR)
    set(CMAKE_INSTALL_PREFIX ${DESTDIR}${CMAKE_INSTALL_PREFIX})
endif()
install(TARGETS futurerestore
        DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")

