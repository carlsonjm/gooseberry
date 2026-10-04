# Every test runs in a home of its own (TestHome.h), off screen and with no
# session, so nothing here can read or change a person's notes or desktop.
# In a time zone west of Greenwich, where a date given to the screen as
# midnight in UTC is still the day before.
set(test_environment
    TZ=America/Chicago
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
gooseberry_test(tst_planner core/tst_planner.cpp)
gooseberry_test(tst_ink core/tst_ink.cpp Qt6::Xml)
gooseberry_test(tst_screens screens/tst_screens.cpp
    gooseberry-desktop gooseberry-screensplugin Qt6::Gui Qt6::Quick KF6::I18n KF6::I18nQml)
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

# Reminders as the desktop shows them: the real Gooseberry on a bus of the
# test's own, with the test standing in for the desktop's notifications.
add_executable(tst_reminders_bus bus/tst_reminders_bus.cpp)
target_link_libraries(tst_reminders_bus PRIVATE Qt6::DBus Qt6::Test)
target_compile_definitions(tst_reminders_bus PRIVATE GOOSEBERRY_BINARY="$<TARGET_FILE:gooseberry>")
add_dependencies(tst_reminders_bus gooseberry)
add_test(NAME tst_reminders_bus COMMAND ${DBUS_RUN_SESSION}
    --config-file=${CMAKE_CURRENT_SOURCE_DIR}/bus/private-bus.conf -- $<TARGET_FILE:tst_reminders_bus>)
set_tests_properties(tst_reminders_bus PROPERTIES
    ENVIRONMENT "GOOSEBERRY_PRIVATE_BUS=1"
    SKIP_RETURN_CODE 77
    TIMEOUT 120)

# Reading handwriting: the real reader on a stand-in model of the real one's
# shape, and the reading of notes line by line.
if(TARGET gooseberry-reading)
    gooseberry_test(tst_reading reading/tst_reading.cpp gooseberry-reading Qt6::Gui)
    target_compile_definitions(tst_reading PRIVATE FAKE_READER="${CMAKE_CURRENT_SOURCE_DIR}/reading/fake-reader")

    # The real Gooseberry reading what it has not read, a little after it starts.
    add_executable(tst_reading_bus bus/tst_reading_bus.cpp)
    target_link_libraries(tst_reading_bus PRIVATE Qt6::DBus Qt6::Test)
    target_compile_definitions(tst_reading_bus PRIVATE
        GOOSEBERRY_BINARY="$<TARGET_FILE:gooseberry>"
        FAKE_READER="${CMAKE_CURRENT_SOURCE_DIR}/reading/fake-reader")
    add_dependencies(tst_reading_bus gooseberry)
    add_test(NAME tst_reading_bus COMMAND ${DBUS_RUN_SESSION}
        --config-file=${CMAKE_CURRENT_SOURCE_DIR}/bus/private-bus.conf -- $<TARGET_FILE:tst_reading_bus>)
    set_tests_properties(tst_reading_bus PROPERTIES
        ENVIRONMENT "GOOSEBERRY_PRIVATE_BUS=1;TZ=America/Chicago"
        SKIP_RETURN_CODE 77
        TIMEOUT 120)
endif()
