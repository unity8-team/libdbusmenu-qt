qt4_automoc(slowmenu.cpp)
add_executable(slowmenu slowmenu.cpp)
target_link_libraries(
    slowmenu
    ${QT_QTGUI_LIBRARY}
    ${QT_QTCORE_LIBRARY}
    ${QT_QTDBUS_LIBRARY}
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
    ${CMAKE_CURRENT_BINARY_DIR}
    ${QT_QTTEST_INCLUDE_DIR}
    ${QT_QTDBUS_INCLUDE_DIR}
    )

set(test_LIBRARIES
    ${QT_QTGUI_LIBRARY}
    ${QT_QTCORE_LIBRARY}
    ${QT_QTDBUS_LIBRARY}
    ${QT_QTTEST_LIBRARY}
    dbusmenu-qt
    )

enable_testing()

# dbusmenuexportertest
set(dbusmenuexportertest_SRCS
    dbusmenuexportertest.cpp
    testutils.cpp
    )

qt4_automoc(${dbusmenuexportertest_SRCS})

add_test_executable(dbusmenuexportertest ${dbusmenuexportertest_SRCS})

target_link_libraries(dbusmenuexportertest
    ${test_LIBRARIES}
    )

# dbusmenuimportertest
set(dbusmenuimportertest_SRCS
    dbusmenuimportertest.cpp
    testutils.cpp
    )

qt4_automoc(${dbusmenuimportertest_SRCS})

add_test_executable(dbusmenuimportertest ${dbusmenuimportertest_SRCS})

target_link_libraries(dbusmenuimportertest
    ${test_LIBRARIES}
    )

# dbusmenushortcuttest
set(dbusmenushortcuttest_SRCS
    dbusmenushortcuttest.cpp
    )

qt4_automoc(${dbusmenushortcuttest_SRCS})

add_test_executable(dbusmenushortcuttest ${dbusmenushortcuttest_SRCS})

target_link_libraries(dbusmenushortcuttest
    ${test_LIBRARIES}
    )

# Keep this at the end
create_check_target()
