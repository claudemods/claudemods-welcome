import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts

// Everything fits in the default window size: no scrolling.
Item {
    id: home

    function go(name) {
        ApplicationWindow.window.showPage(name)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // --- logo + title --------------------------------------------------------------
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 14

            Image {
                source: "qrc:/icons/org.claudemods.hello.svg"
                sourceSize.width: 64
                sourceSize.height: 64
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
            }
            Label {
                text: I18n.t("welcome-title")
                font.bold: true
                font.pointSize: Qt.application.font.pointSize * 1.8
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            text: I18n.t("welcome-body")
            wrapMode: Text.WordWrap
            maximumLineCount: 7
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignJustify
        }

        // --- link grid: documentation / support / project ----------------------------
        GridLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            columns: 3
            rowSpacing: 8
            columnSpacing: 10

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: I18n.t("section-docs")
                font.bold: true
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: I18n.t("section-support")
                font.bold: true
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: I18n.t("section-project")
                font.bold: true
            }

            HelloButton {
                Layout.fillWidth: true
                label: I18n.t("button-readme-label")
                focus: true
                onClicked: home.go("readme")
            }
            HelloButton {
                Layout.fillWidth: true
                label: I18n.t("button-forum-label")
                external: true
                tip: I18n.t("button-web-resource-tooltip")
                onClicked: Backend.openUrl("forum")
            }
            HelloButton {
                Layout.fillWidth: true
                label: I18n.t("button-involved-label")
                onClicked: home.go("involved")
            }

            HelloButton {
                Layout.fillWidth: true
                label: I18n.t("button-release-info-label")
                onClicked: home.go("release")
            }
            HelloButton {
                Layout.fillWidth: true
                label: I18n.t("button-software-label")
                external: true
                tip: I18n.t("button-web-resource-tooltip")
                onClicked: Backend.openUrl("software")
            }
            HelloButton {
                Layout.fillWidth: true
                label: I18n.t("button-development-label")
                external: true
                tip: I18n.t("button-web-resource-tooltip")
                onClicked: Backend.openUrl("development")
            }

            HelloButton {
                Layout.fillWidth: true
                label: I18n.t("button-wiki-label")
                external: true
                tip: I18n.t("button-web-resource-tooltip")
                onClicked: Backend.openUrl("wiki")
            }
            Item { Layout.fillWidth: true }
            HelloButton {
                Layout.fillWidth: true
                label: I18n.t("button-donate-label")
                external: true
                tip: I18n.t("button-web-resource-tooltip")
                onClicked: Backend.openUrl("donate")
            }
        }

        Item { Layout.fillHeight: true }

        // --- installer (live ISO only) ----------------------------------------------
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            visible: Backend.isIso
            spacing: 6

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: I18n.t("section-installer")
                font.bold: true
            }
            HelloButton {
                Layout.alignment: Qt.AlignHCenter
                enabled: !Backend.installerBusy
                label: I18n.t("button-installer-label")
                onClicked: Backend.launchInstaller()
            }
        }

        // --- apps / tweaks / troubleshooting (installed systems only) -----------------
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            visible: !Backend.isIso
            spacing: 20

            HelloButton {
                label: I18n.t("tweaksbrowser-label")
                tip: I18n.t("tweaksbrowser-label")
                onClicked: home.go("tweaks")
            }
            HelloButton {
                visible: Backend.hasPackageInstaller
                label: I18n.t("appbrowser-label")
                tip: I18n.t("appbrowser-label")
                onClicked: Backend.launchPackageBrowser()
            }
            HelloButton {
                label: I18n.t("troubleshooting-label")
                tip: I18n.t("troubleshooting-label")
                onClicked: home.go("troubleshooting")
            }
        }

        // --- bottom row: language, social links, autostart -------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ComboBox {
                id: languageBox
                model: Backend.languages
                textRole: "name"
                valueRole: "id"
                Component.onCompleted: currentIndex = Math.max(0, indexOfValue(Backend.locale))
                onActivated: Backend.setLocale(currentValue)
            }

            Item { Layout.fillWidth: true }

            Label { text: I18n.t("launch-start-label") }
            Switch {
                checked: Backend.autostart
                onToggled: Backend.autostart = checked
            }
        }
    }
}
