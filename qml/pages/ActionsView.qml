import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Generic list of actions for one category (data/systemtool/actions.json).
Item {
    id: view

    property string category: ""
    property string pendingId: ""

    readonly property var items: SystemTool.actions(category)

    function trigger(action) {
        if (action.kind === "appimage") {
            appImageDialog.open()
        } else if (action.confirm.length > 0) {
            view.pendingId = action.id
            confirmDialog.text = action.confirm
            confirmDialog.open()
        } else {
            SystemTool.runAction(action.id)
        }
    }

    GridLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, 780)
        columns: 2
        columnSpacing: 16
        rowSpacing: 6

        Repeater {
            model: view.items

            ColumnLayout {
                id: cell
                required property var modelData

                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: 1

                HelloButton {
                    Layout.fillWidth: true
                    label: cell.modelData.title
                    onClicked: view.trigger(cell.modelData)
                }
                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 4
                    text: cell.modelData.description
                    wrapMode: Text.WordWrap
                    opacity: 0.75
                    font.pointSize: Qt.application.font.pointSize * 0.82
                }
            }
        }
    }

    Dialog {
        id: confirmDialog
        property alias text: confirmLabel.text
        anchors.centerIn: Overlay.overlay
        modal: true
        width: Math.min(view.width - 40, 460)
        title: "Are you sure?"
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: SystemTool.runAction(view.pendingId)

        contentItem: Label {
            id: confirmLabel
            wrapMode: Text.WordWrap
        }
    }

    FileDialog {
        id: appImageDialog
        title: "Choose an AppImage"
        nameFilters: ["AppImage (*.AppImage *.appimage)", "All files (*)"]
        onAccepted: SystemTool.integrateAppImage(selectedFile)
    }
}
