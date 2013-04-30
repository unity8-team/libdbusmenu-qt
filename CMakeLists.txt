project(dbusmenu-qt)
cmake_minimum_required(VERSION 2.6.0)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")

# Build options
option(WITH_DOC "Build documentation (requires Doxygen)" ON)

# Versions
## Package version
set(dbusmenu_qt_VERSION_MAJOR 0)
set(dbusmenu_qt_VERSION_MINOR 9)
set(dbusmenu_qt_VERSION_PATCH 2)
set(dbusmenu_qt_VERSION ${dbusmenu_qt_VERSION_MAJOR}.${dbusmenu_qt_VERSION_MINOR}.${dbusmenu_qt_VERSION_PATCH})

## Lib version
### Bump this one when a binary-incompatible change is introduced
set(dbusmenu_qt_lib_SOVERSION 2)

### Bump this one when the API is extended in a binary-compatible way
set(dbusmenu_qt_lib_API_VERSION 6)

### Bump this one when changes do not extend the API
set(dbusmenu_qt_lib_PATCH_VERSION 0)

set(dbusmenu_qt_lib_VERSION ${dbusmenu_qt_lib_SOVERSION}.${dbusmenu_qt_lib_API_VERSION}.${dbusmenu_qt_lib_PATCH_VERSION})

# dist targets
set(ARCHIVE_NAME libdbusmenu-qt-${dbusmenu_qt_VERSION})
add_custom_target(dist
    COMMAND bzr export --root=${ARCHIVE_NAME} ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

add_custom_target(distcheck
    COMMAND cd ${CMAKE_BINARY_DIR}
    && rm -rf ${ARCHIVE_NAME}
    && tar xf ${ARCHIVE_NAME}.tar.bz2
    && mkdir ${ARCHIVE_NAME}/build
    && cd ${ARCHIVE_NAME}/build
    && cmake -DCMAKE_INSTALL_PREFIX=../install ..
    && make
    && make install
    && make check
    )
add_dependencies(distcheck dist)

option(QT4_BUILD "Force building with Qt4 even if Qt5 is found")
if (NOT QT4_BUILD)
    find_package(Qt5Widgets QUIET)
endif()

if (Qt5Widgets_FOUND)
   find_package(Qt5DBus REQUIRED)
   include_directories(${Qt5Widgets_INCLUDE_DIRS} ${Qt5DBus_INCLUDE_DIRS})
   find_package(ECM 0.0.4 REQUIRED)
   set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})
   find_package(Qt5Transitional REQUIRED Core)
   set(CMAKE_AUTOMOC ON)
   set(CMAKE_AUTOMOC_RELAXED_MODE ON)
   include(GNUInstallDirs)
   set(LIB_DESTINATION "${CMAKE_INSTALL_LIBDIR}")
else()
  find_package(Qt4 REQUIRED)
  include_directories(
    ${QT_INCLUDE_DIR}
    ${QT_QTCORE_INCLUDE_DIR}
    ${QT_QTDBUS_INCLUDE_DIR}
    ${QT_QTGUI_INCLUDE_DIR}
    )
  set(LIB_DESTINATION "lib${LIB_SUFFIX}")
endif()


configure_file(dbusmenu-qt.pc.in ${CMAKE_BINARY_DIR}/dbusmenu-qt.pc @ONLY)

install(FILES ${CMAKE_BINARY_DIR}/dbusmenu-qt.pc
        DESTINATION ${LIB_DESTINATION}/pkgconfig
    )

add_subdirectory(src)
if (NOT Qt5Widgets_FOUND) # TODO port tests to Qt5
add_subdirectory(tests)
endif()
add_subdirectory(tools)

if(WITH_DOC)
    configure_file(Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile @ONLY)

    add_custom_target(doc ALL doxygen
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

    install(DIRECTORY ${CMAKE_BINARY_DIR}/html/
        DESTINATION share/doc/dbusmenu-qt
        )
endif(WITH_DOC)
