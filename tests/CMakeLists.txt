qt5_wrap_cpp(slowmenu_MOC_SRCS slowmenu.h)
add_executable(slowmenu slowmenu.cpp ${slowmenu_MOC_SRCS})
target_link_libraries(
    slowmenu
    ${Qt5Gui_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Core_LIBRARIES}
    ${Qt5DBus_LIBRARIES}
    dbusmenu-qt
    )

# Macros to create "check" target
set(_test_executable_list "")

macro(add_test_executable _executable)
    add_test(${_executable} ${_executable})
    set(_test_executable_list "${_test_executable_list};${_executable}")
    add_executable(${_executable} ${ARGN})
endmacro(add_test_executable)

# Call this at the end
macro(create_check_target)
    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose
        DEPENDS ${_test_executable_list})
endmacro(create_check_target)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_BINARY_DIR}/../src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Qt5Gui_INCLUDE_DIRS}
    ${Qt5Test_INCLUDE_DIRS}
    ${Qt5DBus_INCLUDE_DIRS}
    )

set(test_LIBRARIES
    ${Qt5Gui_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Core_LIBRARIES}
    ${Qt5DBus_LIBRARIES}
    ${Qt5Test_LIBRARIES}
    dbusmenu-qt
    )

enable_testing()

# dbusmenuexportertest
set(dbusmenuexportertest_SRCS
    dbusmenuexportertest.cpp
    testutils.cpp
    )

set(dbusmenuexportertest_MOC_HEADERS
    dbusmenuexportertest.h
    testutils.h
    )

qt5_wrap_cpp(dbusmenuexportertest_MOC_SRCS ${dbusmenuexportertest_MOC_HEADERS})

add_test_executable(dbusmenuexportertest ${dbusmenuexportertest_SRCS} ${dbusmenuexportertest_MOC_SRCS})

target_link_libraries(dbusmenuexportertest
    ${test_LIBRARIES}
    )

# dbusmenuimportertest
set(dbusmenuimportertest_SRCS
    dbusmenuimportertest.cpp
    testutils.cpp
    )

set(dbusmenuimportertest_MOC_HEADERS
    dbusmenuimportertest.h
    testutils.h
    )

qt5_wrap_cpp(dbusmenuimportertest_MOC_SRCS ${dbusmenuimportertest_MOC_HEADERS})

add_test_executable(dbusmenuimportertest ${dbusmenuimportertest_SRCS} ${dbusmenuimportertest_MOC_SRCS})

target_link_libraries(dbusmenuimportertest
    ${test_LIBRARIES}
    )

# dbusmenushortcuttest
set(dbusmenushortcuttest_SRCS
    dbusmenushortcuttest.cpp
    )

set(dbusmenushortcuttest_MOC_HEADERS
    dbusmenushortcuttest.h
    )

qt5_wrap_cpp(dbusmenushortcuttest_MOC_SRCS ${dbusmenushortcuttest_MOC_HEADERS})

add_test_executable(dbusmenushortcuttest ${dbusmenushortcuttest_SRCS} ${dbusmenushortcuttest_MOC_SRCS})

target_link_libraries(dbusmenushortcuttest
    ${test_LIBRARIES}
    )

# Keep this at the end
create_check_target()
