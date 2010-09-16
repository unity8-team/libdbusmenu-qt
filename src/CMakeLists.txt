include(CheckCXXSourceCompiles)
include (CheckCXXCompilerFlag)

# Check some compiler flags
check_cxx_compiler_flag(-fvisibility=hidden __DBUSMENU_HAVE_GCC_VISIBILITY)
if (__DBUSMENU_HAVE_GCC_VISIBILITY AND NOT WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif (__DBUSMENU_HAVE_GCC_VISIBILITY AND NOT WIN32)

check_cxx_compiler_flag(-Woverloaded-virtual __DBUSMENU_HAVE_W_OVERLOADED_VIRTUAL)
if (__DBUSMENU_HAVE_W_OVERLOADED_VIRTUAL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual")
endif (__DBUSMENU_HAVE_W_OVERLOADED_VIRTUAL)

check_cxx_compiler_flag(-Wall __DBUSMENU_HAVE_W_ALL)
if (__DBUSMENU_HAVE_W_ALL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif (__DBUSMENU_HAVE_W_ALL)

# Check whether QIcon::name() exists. It was added in late Qt 4.7 cycle, and is
# not present in betas.
set(CMAKE_REQUIRED_INCLUDES "${QT_INCLUDE_DIR}")
set(CMAKE_REQUIRED_LIBRARIES "${QT_QTGUI_LIBRARIES};${QT_QTCORE_LIBRARIES}")
check_cxx_source_compiles("
#include <QtGui/QIcon>
int main() {
    QIcon icon;
    icon.name();
    return 0;
}
" HAVE_QICON_NAME)
if (NOT HAVE_QICON_NAME)
    message(STATUS "QIcon::name() does not exist, DBusMenuExporter will not export icon names by itself")
endif()
configure_file(dbusmenu_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/dbusmenu_config.h @ONLY)

set(dbusmenu_qt_SRCS
    dbusmenu_p.cpp
    dbusmenuexporter.cpp
    dbusmenuexporterdbus_p.cpp
    dbusmenuimporter.cpp
    dbusmenuitem_p.cpp
    dbusmenushortcut_p.cpp
    utils.cpp
    )

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
    )

qt4_automoc(${dbusmenu_qt_SRCS})

qt4_add_dbus_adaptor(dbusmenu_qt_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/org.ayatana.dbusmenu.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/dbusmenuexporterdbus_p.h DBusMenuExporterDBus
    )

configure_file(dbusmenu_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/dbusmenu_version.h
    )

add_library(dbusmenu-qt SHARED ${dbusmenu_qt_SRCS})
set_target_properties(dbusmenu-qt PROPERTIES
    VERSION ${dbusmenu_qt_lib_VERSION}
    SOVERSION ${dbusmenu_qt_lib_SOVERSION}
    )

target_link_libraries(dbusmenu-qt
    ${QT_QTGUI_LIBRARIES}
    ${QT_QTDBUS_LIBRARIES}
    ${QT_QTXML_LIBRARIES}
    ${QT_QTCORE_LIBRARIES}
    )

install(TARGETS dbusmenu-qt
    LIBRARY DESTINATION lib${LIB_SUFFIX})

install(DIRECTORY .
    DESTINATION include/dbusmenu-qt
    FILES_MATCHING PATTERN "*.h"
    PATTERN "*_p.h" EXCLUDE
    )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dbusmenu_version.h
    DESTINATION include/dbusmenu-qt/
    )
