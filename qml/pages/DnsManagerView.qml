import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts

// claudemods DNS Manager: pick a provider, apply it to a connection, see what is in use, reset to automatic.
Item {
    id: view

    readonly property var providers: DnsManager.providers()
    readonly property var connections: DnsManager.connections()
    property string provider: ""
    property string connection: ""

    Component.onCompleted: {
        const active = DnsManager.activeConnection()
        const idx = connections.indexOf(active)
        connection = idx >= 0 ? active : (connections.length > 0 ? connections[0] : "")
        connectionBox.currentIndex = Math.max(0, connections.indexOf(connection))
        DnsManager.refresh(connection)
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 8

        // --- connection + current DNS -------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label { text: "Connection" }
            ComboBox {
                id: connectionBox
                Layout.fillWidth: true
                model: view.connections
                onActivated: index => {
                    view.connection = view.connections[index]
                    DnsManager.refresh(view.connection)
                }
            }
            ToolButton {
                text: "⟳"
                Accessible.name: "Refresh"
                ToolTip.visible: hovered
                ToolTip.text: "Refresh the DNS in use"
                onClicked: DnsManager.refresh(view.connection)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 40
            radius: 8
            color: "#22ffffff"
            border.color: "#33ffffff"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 0
                Label {
                    text: "CURRENT DNS"
                    font.pointSize: Qt.application.font.pointSize * 0.7
                    font.letterSpacing: 1.2
                    opacity: 0.6
                }
                Label {
                    Layout.fillWidth: true
                    text: DnsManager.currentDns
                    elide: Text.ElideRight
                    font.pointSize: Qt.application.font.pointSize * 0.9
                }
            }
        }

        Label {
            text: "PROVIDERS"
            font.pointSize: Qt.application.font.pointSize * 0.75
            font.letterSpacing: 1.2
            opacity: 0.6
        }

        // --- provider tiles ------------------------------------------------------------------
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 8
            rowSpacing: 8

            Repeater {
                model: view.providers

                Rectangle {
                    id: tile
                    required property var modelData
                    readonly property bool selected: view.provider === modelData.name

                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: 62
                    radius: 8
                    color: selected ? "#331fb59a" : (tileMouse.containsMouse ? "#38ffffff" : "#22ffffff")
                    border.width: selected ? 2 : 1
                    border.color: selected || tileMouse.containsMouse ? "#1fb59a" : "#33ffffff"
                    activeFocusOnTab: true
                    Keys.onReturnPressed: view.provider = modelData.name
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData.name
                    Accessible.checked: selected

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 1
                        Label {
                            text: tile.modelData.name
                            font.bold: true
                            font.pointSize: Qt.application.font.pointSize * 0.95
                        }
                        Label {
                            Layout.fillWidth: true
                            text: tile.modelData.ipv4
                            elide: Text.ElideRight
                            opacity: 0.75
                            font.pointSize: Qt.application.font.pointSize * 0.8
                        }
                        Label {
                            Layout.fillWidth: true
                            text: tile.modelData.ipv6
                            elide: Text.ElideRight
                            opacity: 0.55
                            font.pointSize: Qt.application.font.pointSize * 0.75
                        }
                    }
                    MouseArea {
                        id: tileMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: view.provider = tile.modelData.name
                    }
                }
            }
        }

        // --- options + actions -----------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 10

            CheckBox {
                id: dotBox
                text: "DNS over TLS (encrypted)"
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "Reset to automatic"
                onClicked: DnsManager.reset(view.connection)
            }
            Button {
                text: "Apply"
                highlighted: true
                enabled: view.provider.length > 0 && view.connection.length > 0
                onClicked: DnsManager.apply(view.connection, view.provider, dotBox.checked)
            }
        }
    }
}
