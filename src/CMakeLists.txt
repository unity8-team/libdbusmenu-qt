include (CheckCXXCompilerFlag)
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

set(dbusmenu_qt_SRCS
    dbusmenu_p.cpp
    dbusmenuexporter.cpp
    dbusmenuimporter.cpp
    dbusmenuitem.cpp
    utils.cpp
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
