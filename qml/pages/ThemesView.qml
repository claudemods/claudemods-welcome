import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts

// Wallpaper photo grid and named colour chips. Same commands as the old Theme Changer:
// plasma-apply-wallpaperimage / plasma-apply-colorscheme.
Item {
    id: view

    property var colors: SystemTool.colorSchemes()

    Connections {
        target: SystemTool
        function onColorSchemesChanged() {
            view.colors = SystemTool.colorSchemes()
        }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 6

        Label {
            visible: !SystemTool.hasPlasmaTools
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#ffb86c"
            font.pointSize: Qt.application.font.pointSize * 0.85
            text: "KDE Plasma tools (plasma-apply-wallpaperimage / plasma-apply-colorscheme) were not found: " +
                  "the buttons below need a Plasma desktop."
        }

        Label {
            text: "WALLPAPERS"
            font.pointSize: Qt.application.font.pointSize * 0.75
            font.letterSpacing: 1.2
            opacity: 0.6
        }

        // --- wallpaper photo buttons (5 per row) -------------------------------------------
        GridLayout {
            Layout.fillWidth: true
            columns: 5
            columnSpacing: 8
            rowSpacing: 8

            Repeater {
                model: SystemTool.wallpapers()

                Item {
                    id: photo
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: width * 9 / 16
                    opacity: modelData.available ? 1.0 : 0.45
                    activeFocusOnTab: true
                    Keys.onReturnPressed: if (modelData.available) SystemTool.applyWallpaper(modelData.id)
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.name

                    Image {
                        id: pic
                        anchors.fill: parent
                        source: photo.modelData.thumb
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        smooth: true
                    }
                    Rectangle { // caption strip
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 20
                        color: "#aa000000"
                        Label {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            verticalAlignment: Text.AlignVCenter
                            text: photo.modelData.name
                            color: "white"
                            font.bold: true
                            font.pointSize: Qt.application.font.pointSize * 0.8
                            elide: Text.ElideRight
                        }
                    }
                    Rectangle { // hover / focus frame
                        anchors.fill: parent
                        color: "transparent"
                        border.width: (photoMouse.containsMouse || photo.activeFocus) ? 2 : 1
                        border.color: (photoMouse.containsMouse || photo.activeFocus) ? "#1fb59a" : "#44ffffff"
                        radius: 3
                    }
                    MouseArea {
                        id: photoMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: photo.modelData.available
                        cursorShape: Qt.PointingHandCursor
                        onClicked: SystemTool.applyWallpaper(photo.modelData.id)
                    }
                    ToolTip.visible: photoMouse.containsMouse
                    ToolTip.delay: 450
                    ToolTip.text: photo.modelData.available ? "Set \"" + photo.modelData.name + "\" as the wallpaper"
                                                            : "This wallpaper file is not installed"
                }
            }
        }

        Label {
            Layout.topMargin: 6
            text: "COLOURS"
            font.pointSize: Qt.application.font.pointSize * 0.75
            font.letterSpacing: 1.2
            opacity: 0.6
        }

        // --- named colour chips (5 per row) -------------------------------------------------
        GridLayout {
            Layout.fillWidth: true
            columns: 5
            columnSpacing: 6
            rowSpacing: 6

            Repeater {
                model: view.colors

                Rectangle {
                    id: chip
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: 30
                    radius: 6
                    color: chipMouse.pressed ? "#55ffffff" : chipMouse.containsMouse ? "#38ffffff" : "#22ffffff"
                    border.width: 1
                    border.color: chipMouse.containsMouse ? "#1fb59a" : "#33ffffff"
                    activeFocusOnTab: true
                    Keys.onReturnPressed: SystemTool.applyColorScheme(modelData.id)
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.name

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 6

                        // two-tone chip: window background | accent colour
                        Row {
                            Rectangle { width: 11; height: 18; radius: 2; color: chip.modelData.bg; border.color: "#77ffffff" }
                            Rectangle { width: 11; height: 18; radius: 2; color: chip.modelData.accent; border.color: "#77ffffff" }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: chip.modelData.name
                            elide: Text.ElideRight
                            font.pointSize: Qt.application.font.pointSize * 0.9
                        }
                    }
                    MouseArea {
                        id: chipMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: SystemTool.applyColorScheme(chip.modelData.id)
                    }
                    ToolTip.visible: chipMouse.containsMouse
                    ToolTip.delay: 450
                    ToolTip.text: "Apply the colour scheme \"" + modelData.name + "\""
                }
            }
        }
    }
}
