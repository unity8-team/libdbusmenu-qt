set(qtapp_SRCS
    main.cpp
    )

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_BINARY_DIR}/../../src
    ${Qt5Core_INCLUDE_DIRS}
    ${Qt5Gui_INCLUDE_DIRS}
    ${Qt5Widgets_INCLUDE_DIRS}
    ${Qt5DBus_INCLUDE_DIRS}
    ${QJSON_INCLUDE_DIR}
    )

add_executable(dbusmenubench-qtapp ${qtapp_SRCS})

target_link_libraries(dbusmenubench-qtapp
    dbusmenu-qt
    ${Qt5Gui_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Core_LIBRARIES}
    ${Qt5DBus_LIBRARIES}
    ${QJSON_LIBRARY}
    )
