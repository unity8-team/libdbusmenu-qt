set(qtapp_SRCS
    main.cpp
    )

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_BINARY_DIR}/../../src
    ${QT_INCLUDE_DIR}
    ${QT_QTCORE_INCLUDE_DIR}
    ${QT_QTGUI_INCLUDE_DIR}
    ${QT_QTDBUS_INCLUDE_DIR}
    ${QJSON_INCLUDE_DIR}
    )

add_executable(dbusmenubench-qtapp ${qtapp_SRCS})

target_link_libraries(dbusmenubench-qtapp
    dbusmenu-qt
    ${QT_QTGUI_LIBRARY}
    ${QT_QTCORE_LIBRARY}
    ${QT_QTDBUS_LIBRARY}
    ${QJSON_LIBRARIES}
    )
