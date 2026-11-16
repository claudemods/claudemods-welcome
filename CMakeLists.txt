cmake_minimum_required(VERSION 3.21)
project(claudemods-hello VERSION 0.28.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 DBus Network Concurrent)
qt_standard_project_setup(REQUIRES 6.5)

# ---------------------------------------------------------------------------
# Embedded data: preferences, translations (.ftl), info pages, images, icons
# ---------------------------------------------------------------------------
file(GLOB_RECURSE HELLO_FTL   RELATIVE "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/i18n/*.ftl")
file(GLOB_RECURSE HELLO_PAGES RELATIVE "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/data/pages/*")
file(GLOB         HELLO_IMG   RELATIVE "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/data/img/*.svg" "${CMAKE_SOURCE_DIR}/data/img/*.webp")
file(GLOB         HELLO_ICONS RELATIVE "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/icons/*.svg")
file(GLOB         HELLO_THUMBS  RELATIVE "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/data/img/wallpapers/*.jpg")
file(GLOB         HELLO_COLORS  RELATIVE "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/data/color-schemes/*.colors")
file(GLOB_RECURSE HELLO_SYSTOOL RELATIVE "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/data/systemtool/*")
set(HELLO_RESOURCE_FILES data/preferences.json ${HELLO_FTL} ${HELLO_PAGES} ${HELLO_IMG} ${HELLO_ICONS}
    ${HELLO_THUMBS} ${HELLO_COLORS} ${HELLO_SYSTOOL})

qt_add_executable(claudemods-hello
    src/main.cpp
    src/backend.h        src/backend.cpp
    src/actions.h        src/actions.cpp
    src/cli.h            src/cli.cpp
    src/dnsdata.h        src/dnsdata.cpp
    src/dnsmanager.h     src/dnsmanager.cpp
    src/fluent.h         src/fluent.cpp
    src/logger.h         src/logger.cpp
    src/networkmanager.h src/networkmanager.cpp
    src/systemd.h        src/systemd.cpp
    src/systemtool.h     src/systemtool.cpp
    src/tweak.h          src/tweak.cpp
    src/utils.h          src/utils.cpp
    src/externals.h
)

# The generated QML type registration file #includes "backend.h"; it must be findable.
target_include_directories(claudemods-hello PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

set_source_files_properties(qml/I18n.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(claudemods-hello
    URI ClaudeMods.Hello
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/I18n.qml
        qml/components/HelloButton.qml
        qml/components/BackBar.qml
        qml/components/ToolCard.qml
        qml/pages/HomePage.qml
        qml/pages/TextPage.qml
        qml/pages/TweaksPage.qml
        qml/pages/TroubleshootingPage.qml
        qml/pages/DnsPage.qml
        qml/pages/SystemToolPage.qml
        qml/pages/ThemesView.qml
        qml/pages/ActionsView.qml
        qml/pages/DnsManagerView.qml
)

qt_add_resources(claudemods-hello "hello_data"
    PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}"
    FILES ${HELLO_RESOURCE_FILES}
)

target_compile_definitions(claudemods-hello PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    CLAUDEMODS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(claudemods-hello PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::DBus Qt6::Network Qt6::Concurrent)

include(GNUInstallDirs)
install(TARGETS claudemods-hello RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES claudemods-hello.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES icons/org.claudemods.hello.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(DIRECTORY data/wallpapers/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/claudemods-hello/wallpapers)
install(PROGRAMS scripts/terminal-helper scripts/rootshell.sh
        DESTINATION ${CMAKE_INSTALL_DATADIR}/claudemods-hello/scripts)
