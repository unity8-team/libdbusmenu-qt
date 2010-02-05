set(dbusmenu_qt_SRCS
    dbusmenuexporter.cpp
    )

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
    )

qt4_automoc(${dbusmenu_qt_SRCS})

qt4_add_dbus_adaptor(dbusmenu_qt_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/org.ayatana.dbusmenu.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/dbusmenuexporter.h DBusMenuExporter
    )

add_library(dbusmenu-qt SHARED ${dbusmenu_qt_SRCS})
set_target_properties(dbusmenu-qt PROPERTIES
    VERSION ${dbusmenu_qt_lib_VERSION}
    SOVERSION ${dbusmenu_qt_lib_SOVERSION}
    )

target_link_libraries(dbusmenu-qt
    ${QT_QTGUI_LIBRARIES}
    ${QT_QTDBUS_LIBRARIES}
    ${QT_QTCORE_LIBRARIES}
    )

install(TARGETS dbusmenu-qt
    LIBRARY DESTINATION lib${LIB_SUFFIX})

install(DIRECTORY .
    DESTINATION include/libdbusmenu-qt
    FILES_MATCHING PATTERN "*.h"
    PATTERN "*_p.h" EXCLUDE
    )
