import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts

// Compact clickable tile: bold title with a one/two line description underneath.
Rectangle {
    id: card

    property string title: ""
    property string subtitle: ""

    signal clicked()

    implicitHeight: 52
    radius: 8
    color: mouse.pressed ? "#55ffffff" : (mouse.containsMouse || activeFocus) ? "#38ffffff" : "#22ffffff"
    border.width: 1
    border.color: (mouse.containsMouse || activeFocus) ? "#1fb59a" : "#33ffffff"

    Behavior on color { ColorAnimation { duration: 90 } }
    Behavior on border.color { ColorAnimation { duration: 90 } }

    activeFocusOnTab: true
    Keys.onReturnPressed: card.clicked()
    Keys.onSpacePressed: card.clicked()
    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.onPressAction: card.clicked()

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 8
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        spacing: 1

        Label {
            Layout.fillWidth: true
            text: card.title
            font.bold: true
            font.pointSize: Qt.application.font.pointSize * 0.95
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            text: card.subtitle
            visible: text.length > 0
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            opacity: 0.7
            font.pointSize: Qt.application.font.pointSize * 0.8
        }
        Item { Layout.fillHeight: true }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: card.clicked()
    }
}
