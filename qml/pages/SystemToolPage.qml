import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts

// claudemods System Tool: sidebar with every section, content grid on the right.
// The arrow at the top of the sidebar goes back to Apps/Tweaks.
Item {
    id: page

    /// category id from data/systemtool/actions.json
    property string section: "themes"

    readonly property var categories: SystemTool.categories()
    readonly property var current: {
        for (let i = 0; i < categories.length; ++i)
            if (categories[i].id === section)
                return categories[i]
        return categories.length > 0 ? categories[0] : null
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 10

        // --- sidebar ----------------------------------------------------------------------
        Rectangle {
            Layout.preferredWidth: 176
            Layout.fillHeight: true
            radius: 10
            color: "#55000000"
            border.color: "#22ffffff"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true
                    ToolButton {
                        text: "←"
                        font.pointSize: Qt.application.font.pointSize * 1.2
                        Accessible.name: "Back to Apps/Tweaks"
                        ToolTip.visible: hovered
                        ToolTip.text: "Back to Apps/Tweaks"
                        onClicked: ApplicationWindow.window.showPage("tweaks")
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "System Tool"
                        font.bold: true
                        elide: Text.ElideRight
                    }
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#22ffffff" }

                Repeater {
                    model: page.categories

                    ItemDelegate {
                        id: nav
                        required property var modelData

                        Layout.fillWidth: true
                        implicitHeight: 34
                        text: modelData.title
                        icon.name: modelData.icon
                        icon.width: 18
                        icon.height: 18
                        highlighted: page.section === modelData.id
                        font.pointSize: Qt.application.font.pointSize * 0.95
                        onClicked: page.section = modelData.id
                    }
                }

                Item { Layout.fillHeight: true }

                Label {
                    Layout.fillWidth: true
                    Layout.margins: 6
                    text: "claudemods"
                    opacity: 0.45
                    font.pointSize: Qt.application.font.pointSize * 0.75
                }
            }
        }

        // --- content ----------------------------------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 6

            ColumnLayout {
                spacing: 0
                Label {
                    text: page.current ? page.current.title : ""
                    font.bold: true
                    font.pointSize: Qt.application.font.pointSize * 1.25
                }
                Label {
                    text: page.current ? page.current.description : ""
                    opacity: 0.65
                    font.pointSize: Qt.application.font.pointSize * 0.85
                }
            }

            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                active: page.current !== null
                sourceComponent: !page.current ? null
                                 : page.current.view === "themes" ? themesComponent
                                 : page.current.view === "dns" ? dnsComponent : actionsComponent
            }
        }
    }

    Component {
        id: themesComponent
        ThemesView {}
    }
    Component {
        id: dnsComponent
        DnsManagerView {}
    }
    Component {
        id: actionsComponent
        ActionsView { category: page.section }
    }
}
