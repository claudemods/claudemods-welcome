import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts

// Desktop wallpapers (photo buttons) and named colour schemes. Same commands as the old Theme
// Changer: plasma-apply-wallpaperimage / plasma-apply-colorscheme.
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
        anchors.fill: parent
        spacing: 6

        Label {
            visible: !SystemTool.hasPlasmaTools
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#ffb86c"
            text: "KDE Plasma tools (plasma-apply-wallpaperimage / plasma-apply-colorscheme) were not found, so the " +
                  "buttons below will not work on this desktop."
        }

        Label {
            text: "Wallpapers"
            font.bold: true
        }

        // --- photo buttons -----------------------------------------------------------------
        Flow {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: SystemTool.wallpapers()

                Button {
                    id: photo
                    required property var modelData

                    width: 150
                    padding: 4
                    enabled: modelData.available
                    onClicked: SystemTool.applyWallpaper(modelData.id)

                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: modelData.available ? "Set \"" + modelData.name + "\" as the desktop wallpaper"
                                                      : "The wallpaper file is not installed"

                    contentItem: ColumnLayout {
                        spacing: 2
                        Image {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 142
                            Layout.preferredHeight: 80
                            source: photo.modelData.thumb
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            smooth: true
                        }
                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: photo.modelData.name
                            font.bold: true
                        }
                    }
                }
            }
        }

        Label {
            Layout.topMargin: 4
            text: "Colours"
            font.bold: true
        }

        // --- colour buttons (named) --------------------------------------------------------
        Flow {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: view.colors

                Button {
                    id: swatch
                    required property var modelData

                    leftPadding: 8
                    rightPadding: 12
                    topPadding: 5
                    bottomPadding: 5
                    onClicked: SystemTool.applyColorScheme(modelData.id)

                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: "Apply the colour scheme \"" + modelData.name + "\""

                    contentItem: RowLayout {
                        spacing: 8
                        // two-tone chip: window background | accent colour
                        Row {
                            Rectangle { width: 14; height: 22; radius: 3; color: swatch.modelData.bg; border.color: "#66ffffff" }
                            Rectangle { width: 14; height: 22; radius: 3; color: swatch.modelData.accent; border.color: "#66ffffff" }
                        }
                        Label { text: swatch.modelData.name }
                    }
                }
            }
        }

        RowLayout {
            Layout.topMargin: 6
            spacing: 10

            HelloButton {
                label: "Install all colour schemes"
                tip: "Copies the bundled colour schemes to ~/.local/share/color-schemes"
                onClicked: SystemTool.installColorSchemes()
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.75
                font.pointSize: Qt.application.font.pointSize * 0.85
                text: "Colour buttons install their scheme automatically when needed; this installs every scheme at once."
            }
        }

        Item { Layout.fillHeight: true }
    }
}
