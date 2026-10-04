# Every test runs in a home of its own (TestHome.h), off screen and with no
# session, so nothing here can read or change a person's notes or desktop.
set(test_environment
    QT_QPA_PLATFORM=offscreen
    QT_QUICK_BACKEND=software
    DBUS_SESSION_BUS_ADDRESS=disabled:
)

function(gooseberry_test name source)
    add_executable(${name} ${source} TestHome.h)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE gooseberry-core Qt6::Test ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "${test_environment}" TIMEOUT 120)
endfunction()

gooseberry_test(tst_note core/tst_note.cpp)
gooseberry_test(tst_store core/tst_store.cpp)
gooseberry_test(tst_keep core/tst_keep.cpp KF6::CoreAddons)
gooseberry_test(tst_screens screens/tst_screens.cpp
    gooseberry-screensplugin Qt6::Gui Qt6::Quick KF6::I18n KF6::I18nQml)
set_target_properties(tst_screens PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN ON)

# The quick note's bus interface, driven by a client that loads none of
# Gooseberry's code, against the real Gooseberry on a bus of the test's own.
find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
add_executable(tst_quicknote_bus bus/tst_quicknote_bus.cpp)
target_link_libraries(tst_quicknote_bus PRIVATE Qt6::DBus Qt6::Test)
target_compile_definitions(tst_quicknote_bus PRIVATE GOOSEBERRY_BINARY="$<TARGET_FILE:gooseberry>")
add_dependencies(tst_quicknote_bus gooseberry)
add_test(NAME tst_quicknote_bus COMMAND ${DBUS_RUN_SESSION}
    --config-file=${CMAKE_CURRENT_SOURCE_DIR}/bus/private-bus.conf -- $<TARGET_FILE:tst_quicknote_bus>)
set_tests_properties(tst_quicknote_bus PROPERTIES
    ENVIRONMENT "GOOSEBERRY_PRIVATE_BUS=1"
    SKIP_RETURN_CODE 77
    TIMEOUT 120)

# The card in Kadunce's card workspace: the real card and board, off screen,
# against a stand-in Kadunce on a bus of the test's own.
add_executable(tst_spread spread/tst_spread.cpp TestHome.h)
target_include_directories(tst_spread PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tst_spread PRIVATE
    gooseberry-desktop gooseberry-screensplugin Qt6::DBus Qt6::Test KF6::I18n KF6::I18nQml)
set_target_properties(tst_spread PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN ON)
add_test(NAME tst_spread COMMAND ${DBUS_RUN_SESSION}
    --config-file=${CMAKE_CURRENT_SOURCE_DIR}/bus/private-bus.conf -- $<TARGET_FILE:tst_spread>)
set_tests_properties(tst_spread PROPERTIES
    ENVIRONMENT "GOOSEBERRY_PRIVATE_BUS=1"
    SKIP_RETURN_CODE 77
    TIMEOUT 120)
