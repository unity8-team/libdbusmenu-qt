project(dbusmenu-qt)
cmake_minimum_required(VERSION 2.6.0)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")

set(dbusmenu_qt_VERSION 0.2.2)

set(ARCHIVE_NAME libdbusmenu-qt-${dbusmenu_qt_VERSION})
add_custom_target(dist
    COMMAND git archive --prefix=${ARCHIVE_NAME}/ HEAD
        | bzip2 > ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2
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

set(dbusmenu_qt_lib_SOVERSION 1)
set(dbusmenu_qt_lib_VERSION ${dbusmenu_qt_lib_SOVERSION}.0.2)

find_package(Qt4 REQUIRED)

include_directories(
    ${QT_INCLUDE_DIR}
    ${QT_QTCORE_INCLUDE_DIR}
    ${QT_QTDBUS_INCLUDE_DIR}
    ${QT_QTXML_INCLUDE_DIR}
    ${QT_QTGUI_INCLUDE_DIR}
    )

configure_file(dbusmenu-qt.pc.in ${CMAKE_BINARY_DIR}/dbusmenu-qt.pc @ONLY)

install(FILES ${CMAKE_BINARY_DIR}/dbusmenu-qt.pc
    DESTINATION lib${LIB_SUFFIX}/pkgconfig
    )

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(tools)
