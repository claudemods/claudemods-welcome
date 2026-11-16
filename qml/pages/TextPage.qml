import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts

// Info page rendered from data/pages/<locale>/<name> (readme, involved) or the live release feed.
Item {
    id: page

    property string name: ""

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        BackBar {}

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            Label {
                width: scroll.availableWidth
                wrapMode: Text.WordWrap
                textFormat: Text.RichText
                text: {
                    const dependsOnLocale = Backend.locale
                    const dependsOnRelease = Backend.releaseText
                    return Backend.pageText(page.name)
                }
                onLinkActivated: link => Backend.openUrl(link)
            }
        }
    }
}
