cmake_minimum_required(VERSION 3.22)

project(gooseberry VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GOOSEBERRY_APP_ID "io.github.carlsonjm.Gooseberry")

find_package(ECM 6.17 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(CTest)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core DBus Gui Qml Quick QuickControls2 Widgets Test)
find_package(KF6 6.17 REQUIRED COMPONENTS CoreAddons DBusAddons I18n Service WindowSystem)
find_package(KF6Kirigami 6.17 REQUIRED)
find_package(LayerShellQt 6.4 REQUIRED)
find_package(LibTaskManager 6.4 REQUIRED)

qt_standard_project_setup(REQUIRES 6.9)

# The core: notes, the folder and the board. It knows nothing of any desktop,
# so a companion on another device can be built on it unchanged.
add_library(gooseberry-core STATIC
    src/core/Note.cpp src/core/Note.h
    src/core/NoteStore.cpp src/core/NoteStore.h
    src/core/Capture.cpp src/core/Capture.h
    src/core/Board.cpp src/core/Board.h
    src/core/Trash.cpp src/core/Trash.h
)
target_include_directories(gooseberry-core PUBLIC src/core)
target_link_libraries(gooseberry-core PUBLIC Qt6::Core)

# The screens, in Kirigami, shared by the program and the screen tests.
qt_add_library(gooseberry-screens STATIC)
qt_add_qml_module(gooseberry-screens
    URI io.github.carlsonjm.gooseberry
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/io/github/carlsonjm/gooseberry
    QML_FILES
        qml/BoardView.qml
        qml/BoardWindow.qml
        qml/CaptureWindow.qml
        qml/Masonry.qml
        qml/NoteCard.qml
        qml/Notice.qml
        qml/Pill.qml
        qml/QuickNote.qml
    RESOURCES
        qml/icons/note.svg
        qml/icons/tuck.svg
)

# What the desktop adds around the core: the window in front, the sheet's
# place on screen, one running copy.
add_library(gooseberry-desktop STATIC
    src/desktop/Log.h
    src/desktop/QuickNoteService.cpp src/desktop/QuickNoteService.h
    src/desktop/Shell.cpp src/desktop/Shell.h
    src/desktop/WindowContext.cpp src/desktop/WindowContext.h
)
target_include_directories(gooseberry-desktop PUBLIC src/desktop)
target_link_libraries(gooseberry-desktop PUBLIC
    gooseberry-core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick
    KF6::I18n KF6::Service KF6::WindowSystem LayerShellQt::Interface PW::LibTaskManager)

qt_add_executable(gooseberry src/app/main.cpp)
# Qt's own QML plugins load at run time; nothing needs linking in statically.
set_target_properties(gooseberry PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN ON)
target_compile_definitions(gooseberry PRIVATE
    GOOSEBERRY_VERSION="${PROJECT_VERSION}"
    GOOSEBERRY_APP_ID="${GOOSEBERRY_APP_ID}")
target_link_libraries(gooseberry PRIVATE
    gooseberry-desktop gooseberry-screensplugin
    Qt6::Widgets Qt6::QuickControls2
    KF6::CoreAddons KF6::DBusAddons KF6::I18n KF6::I18nQml)

install(TARGETS gooseberry ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
configure_file(data/${GOOSEBERRY_APP_ID}.desktop.in ${GOOSEBERRY_APP_ID}.desktop @ONLY)
configure_file(data/${GOOSEBERRY_APP_ID}-autostart.desktop.in ${GOOSEBERRY_APP_ID}-autostart.desktop @ONLY)
configure_file(data/io.github.carlsonjm.gooseberry.service.in io.github.carlsonjm.gooseberry.service @ONLY)
# A search that calls the quick note before Gooseberry has started starts it.
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.carlsonjm.gooseberry.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GOOSEBERRY_APP_ID}.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GOOSEBERRY_APP_ID}-autostart.desktop
    DESTINATION ${KDE_INSTALL_AUTOSTARTDIR} RENAME ${GOOSEBERRY_APP_ID}.desktop)
install(FILES data/${GOOSEBERRY_APP_ID}.svg DESTINATION ${KDE_INSTALL_FULL_ICONDIR}/hicolor/scalable/apps)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
