import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts

// "Back" arrow shown at the top of every sub page.
RowLayout {
    id: bar

    /// page to return to ("home", or "tweaks" for the DNS page)
    property string target: "home"

    Layout.fillWidth: true

    ToolButton {
        text: "←"
        font.pointSize: Qt.application.font.pointSize * 1.3
        Accessible.name: "Back"
        onClicked: ApplicationWindow.window.showPage(bar.target)
    }
    Item { Layout.fillWidth: true }
}
