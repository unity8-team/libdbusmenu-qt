project(dbusmenu-qt)
cmake_minimum_required(VERSION 2.6.0)
set(dbusmenu_qt_VERSION 0.2.0)

set(ARCHIVE_NAME libdbusmenu-qt-${dbusmenu_qt_VERSION})
add_custom_target(dist
    COMMAND git archive --prefix=${ARCHIVE_NAME}/ HEAD
        | bzip2 > ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

set(dbusmenu_qt_lib_SOVERSION 1)
set(dbusmenu_qt_lib_VERSION ${dbusmenu_qt_lib_SOVERSION}.0.0)

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
#add_subdirectory(tests)
