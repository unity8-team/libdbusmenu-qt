project(dbusmenu-qt)
cmake_minimum_required(VERSION 2.6.0)
set(dbusmenu_qt_VERSION 0.1.0)

set(dbusmenu_qt_lib_SOVERSION 0)
set(dbusmenu_qt_lib_VERSION ${dbusmenu_qt_lib_SOVERSION}.0.0)

find_package(Qt4 REQUIRED)

include_directories(
    ${QT_INCLUDE_DIR}
    ${QT_QTCORE_INCLUDE_DIR}
    ${QT_QTDBUS_INCLUDE_DIR}
    ${QT_QTGUI_INCLUDE_DIR}
    )

configure_file(dbusmenu-qt.pc.in ${CMAKE_BINARY_DIR}/dbusmenu-qt.pc @ONLY)

install(FILES ${CMAKE_BINARY_DIR}/dbusmenu-qt.pc
    DESTINATION lib${LIB_SUFFIX}/pkgconfig
    )

add_subdirectory(src)
#add_subdirectory(tests)
