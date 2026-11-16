import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts

// Tools for resolving issues with the system.
Item {
    id: page

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        BackBar {}

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.fillHeight: true
            spacing: 12

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: I18n.t("troubleshooting")
            }

            Flow {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.min(page.width - 40, 640)
                spacing: 10

                HelloButton {
                    label: I18n.t("clear-pkgcache-title")
                    onClicked: Backend.runAction("clear-pkgcache")
                }
                HelloButton {
                    label: I18n.t("remove-lock-title")
                    onClicked: Backend.runAction("remove-lock")
                }
                HelloButton {
                    label: I18n.t("reset-keyrings-title")
                    onClicked: Backend.runAction("reset-keyrings")
                }
                HelloButton {
                    label: I18n.t("reinstall-title")
                    onClicked: Backend.runAction("reinstall")
                }
                HelloButton {
                    visible: Backend.isKwinWayland
                    label: I18n.t("show-kwinw-debug-title")
                    onClicked: Backend.runAction("kwin-debug")
                }
            }
        }
    }
}
