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
