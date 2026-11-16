import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Grid of compact tiles for one category (data/systemtool/actions.json).
Item {
    id: view

    property string category: ""
    property string pendingId: ""

    readonly property var items: SystemTool.actions(category)

    function trigger(action) {
        if (action.kind === "appimage") {
            appImageDialog.open()
        } else if (action.kind === "page") {
            ApplicationWindow.window.showPage(action.page)
        } else if (action.confirm.length > 0) {
            view.pendingId = action.id
            confirmLabel.text = action.confirm
            confirmDialog.open()
        } else {
            SystemTool.runAction(action.id)
        }
    }

    GridLayout {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        columns: 3
        columnSpacing: 8
        rowSpacing: 8

        Repeater {
            model: view.items

            ToolCard {
                required property var modelData

                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                title: modelData.title
                subtitle: modelData.description
                onClicked: view.trigger(modelData)
            }
        }
    }

    Dialog {
        id: confirmDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        width: Math.min(view.width - 40, 440)
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
