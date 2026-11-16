import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls

// Regular button. Set `external` for buttons that open a web page (shows a link arrow).
Button {
    id: control

    property string label: ""
    property bool external: false
    property string tip: ""

    text: external ? label + "  ↗" : label
    leftPadding: 20
    rightPadding: 20
    topPadding: 8
    bottomPadding: 8

    ToolTip.visible: hovered && tip.length > 0
    ToolTip.delay: 500
    ToolTip.text: tip
}
