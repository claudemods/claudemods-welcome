import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: win

    // Large enough that every page fits without scrolling
    width: 860
    height: 580
    minimumWidth: 800
    minimumHeight: 540
    visible: true
    title: "claudemods hello"

    // Dark theme that stays readable on top of the background image
    palette {
        window: "#0b1a16"
        windowText: "#f2f7f5"
        base: "#12251f"
        alternateBase: "#17302a"
        text: "#f2f7f5"
        button: "#1d4a40"
        buttonText: "#f2f7f5"
        highlight: "#1fb59a"
        highlightedText: "#06130f"
        placeholderText: "#9fb8b1"
        link: "#5ee0c8"
        toolTipBase: "#12251f"
        toolTipText: "#f2f7f5"
    }

    background: Item {
        Image {
            anchors.fill: parent
            source: "qrc:/data/img/background.webp"
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }
        Rectangle { // dim the picture so text stays readable
            anchors.fill: parent
            color: "#99000000"
        }
    }

    /// "home", "readme", "release", "involved", "tweaks", "troubleshooting", "dns"
    property string page: "home"
    property var messageQueue: []

    function showPage(name) {
        page = name
    }

    function showNextMessage() {
        if (messageQueue.length === 0 || messageDialog.visible)
            return
        const m = messageQueue.shift()
        messageDialog.title = m.title
        messageText.text = m.text
        messageDialog.open()
    }

    onClosing: Backend.saveSettings()

    Connections {
        target: Backend

        function onMessage(type, text, title) {
            win.messageQueue.push({ type: type, text: text, title: title })
            win.showNextMessage()
        }

        function onRaiseRequested() {
            win.show()
            win.raise()
            win.requestActivate()
        }
    }

    Connections {
        target: SystemTool

        function onMessage(type, text, title) {
            win.messageQueue.push({ type: type, text: text, title: title })
            win.showNextMessage()
        }
    }

    function openAbout() {
        aboutDialog.open()
    }

    Item {
        anchors.fill: parent

        HomePage {
            anchors.fill: parent
            visible: win.page === "home"
        }

        TextPage {
            anchors.fill: parent
            name: "readme"
            visible: win.page === "readme"
        }
        TextPage {
            anchors.fill: parent
            name: "release"
            visible: win.page === "release"
        }
        TextPage {
            anchors.fill: parent
            name: "involved"
            visible: win.page === "involved"
        }

        // Created on demand (they query the system when opened)
        Loader {
            anchors.fill: parent
            active: !Backend.isIso && win.page === "tweaks"
            visible: active
            sourceComponent: TweaksPage {}
        }
        Loader {
            anchors.fill: parent
            active: !Backend.isIso && win.page === "troubleshooting"
            visible: active
            sourceComponent: TroubleshootingPage {}
        }
        Loader {
            anchors.fill: parent
            active: !Backend.isIso && Backend.hasNmcli && win.page === "dns"
            visible: active
            sourceComponent: DnsPage {}
        }
        Loader {
            anchors.fill: parent
            active: !Backend.isIso && win.page === "systemtool"
            visible: active
            sourceComponent: SystemToolPage {}
        }
    }

    Dialog {
        id: messageDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        width: Math.min(win.width - 40, 460)
        standardButtons: Dialog.Ok
        onClosed: Qt.callLater(win.showNextMessage)

        contentItem: Label {
            id: messageText
            wrapMode: Text.WordWrap
        }
    }

    Dialog {
        id: aboutDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        width: Math.min(win.width - 40, 440)
        title: I18n.t("about-dialog-title")
        standardButtons: Dialog.Close

        contentItem: ColumnLayout {
            spacing: 8
            Image {
                Layout.alignment: Qt.AlignHCenter
                source: "qrc:/icons/org.claudemods.hello.svg"
                sourceSize.width: 96
                sourceSize.height: 96
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: I18n.t("about-dialog-title") + " " + Backend.version
                font.bold: true
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: I18n.t("about-dialog-comments")
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "Vladislav Nepogodin"
                opacity: 0.8
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: "© 2021-2026 CachyOS team — Qt/QML port by claudemods\nGNU General Public License v3.0"
                font.pointSize: Qt.application.font.pointSize * 0.85
                opacity: 0.7
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "<a href=\"website\">claudemodsreloaded.co.uk</a> · "
                      + "<a href=\"https://github.com/claudemods\">GitHub</a>"
                textFormat: Text.RichText
                onLinkActivated: link => Backend.openUrl(link)
            }
        }
    }
}
