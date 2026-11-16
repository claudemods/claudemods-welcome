import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts

// "Back" arrow shown at the top of every sub page.
RowLayout {
    id: bar

    /// page to return to ("home", or "tweaks" for the DNS page)
    property string target: "home"
    /// when true the arrow only emits `activated` and the parent decides where to go
    property bool custom: false
    property string title: ""

    signal activated()

    Layout.fillWidth: true

    ToolButton {
        text: "←"
        font.pointSize: Qt.application.font.pointSize * 1.3
        Accessible.name: "Back"
        onClicked: {
            if (bar.custom)
                bar.activated()
            else
                ApplicationWindow.window.showPage(bar.target)
        }
    }
    Label {
        visible: bar.title.length > 0
        text: bar.title
        font.bold: true
        font.pointSize: Qt.application.font.pointSize * 1.2
    }
    Item { Layout.fillWidth: true }
}
