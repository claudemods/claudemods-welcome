import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts

// claudemods System Tool: one hub with every tool behind a section button.
// Back arrow: section -> hub -> Apps/Tweaks.
Item {
    id: page

    /// "" = the hub, otherwise a category id from data/systemtool/actions.json
    property string section: ""

    readonly property var categories: SystemTool.categories()
    readonly property var current: {
        for (let i = 0; i < categories.length; ++i)
            if (categories[i].id === section)
                return categories[i]
        return null
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        BackBar {
            custom: true
            title: page.current ? "claudemods System Tool — " + page.current.title : "claudemods System Tool"
            onActivated: {
                if (page.section !== "")
                    page.section = ""
                else
                    ApplicationWindow.window.showPage("tweaks")
            }
        }

        // --- hub ---------------------------------------------------------------------------
        GridLayout {
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.fillHeight: true
            visible: page.section === ""
            columns: 3
            rowSpacing: 14
            columnSpacing: 14

            Repeater {
                model: page.categories

                ColumnLayout {
                    id: card
                    required property var modelData

                    Layout.preferredWidth: 230
                    spacing: 4

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 96
                        text: card.modelData.title
                        icon.name: card.modelData.icon
                        icon.width: 40
                        icon.height: 40
                        display: AbstractButton.TextUnderIcon
                        font.bold: true
                        font.pointSize: Qt.application.font.pointSize * 1.15
                        onClicked: page.section = card.modelData.id
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: card.modelData.description
                        opacity: 0.8
                        font.pointSize: Qt.application.font.pointSize * 0.88
                    }
                }
            }
        }

        // --- section content ---------------------------------------------------------------
        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: page.current !== null
            sourceComponent: page.current && page.current.view === "themes" ? themesComponent : actionsComponent
        }
    }

    Component {
        id: themesComponent
        ThemesView {}
    }
    Component {
        id: actionsComponent
        ActionsView { category: page.section }
    }
}
