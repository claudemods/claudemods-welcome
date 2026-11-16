import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts

// Apps / Tweaks: systemd tweaks, system utilities and the CachyOS applications.
Item {
    id: page

    // Third-party script: ask before downloading and running it
    Dialog {
        id: christitusDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        width: Math.min(page.width - 40, 460)
        title: I18n.t("christitus-confirm-title")
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: Backend.runAction("christitus")

        contentItem: Label {
            wrapMode: Text.WordWrap
            text: I18n.t("christitus-confirm", { url: "https://christitus.com/linux" })
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        BackBar {}

        Item {
            id: scroll
            property real availableWidth: width
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                width: scroll.availableWidth
                spacing: 20

                // --- tweaks --------------------------------------------------------------
                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: I18n.t("tweaks")
                    }
                    Flow {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: Math.min(scroll.availableWidth - 20, 780)
                        spacing: 16

                        Repeater {
                            model: Backend.tweakList()

                            CheckBox {
                                id: tweakBox
                                required property var modelData

                                text: I18n.t("tweak-enabled-title", { tweak: modelData.label })
                                checked: Backend.tweakEnabled(modelData.id)
                                onToggled: Backend.toggleTweak(modelData.id)

                                ToolTip.visible: hovered
                                ToolTip.delay: 500
                                ToolTip.text: I18n.t(modelData.tooltipId)

                                // re-read the real state once the (possibly failed) toggle finished
                                Connections {
                                    target: Backend
                                    function onTweaksChanged() {
                                        tweakBox.checked = Backend.tweakEnabled(tweakBox.modelData.id)
                                    }
                                }
                            }
                        }
                    }
                }

                // --- utilities ------------------------------------------------------------
                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: I18n.t("fixes")
                    }
                    Flow {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: Math.min(scroll.availableWidth - 20, 780)
                        spacing: 10

                        HelloButton {
                            label: I18n.t("update-system-title")
                            onClicked: Backend.runAction("update-system")
                        }
                        HelloButton {
                            label: I18n.t("remove-orphans-title")
                            onClicked: Backend.runAction("remove-orphans")
                        }
                        HelloButton {
                            label: I18n.t("rankmirrors-title")
                            onClicked: Backend.runAction("rankmirrors")
                        }
                        HelloButton {
                            label: I18n.t("install-gaming-title")
                            onClicked: Backend.runAction("install-gaming")
                        }
                        HelloButton {
                            label: I18n.t("install-winboat-title")
                            onClicked: Backend.runAction("install-winboat")
                        }
                        HelloButton {
                            visible: Backend.hasVramButton
                            label: I18n.t("install-vram-management-title")
                            tip: I18n.t("install-vram-management-tooltip")
                            onClicked: Backend.runAction("install-vram-management")
                        }
                        HelloButton {
                            label: I18n.t("christitus-title")
                            tip: I18n.t("christitus-tooltip")
                            onClicked: christitusDialog.open()
                        }
                        HelloButton {
                            visible: Backend.hasNmcli
                            label: I18n.t("dnsserver-title")
                            onClicked: ApplicationWindow.window.showPage("dns")
                        }
                    }
                }

                // --- applications ---------------------------------------------------------
                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    visible: Backend.hasPackageInstaller || Backend.hasKernelManager
                    spacing: 8

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: I18n.t("applications")
                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 10

                        HelloButton {
                            visible: Backend.hasPackageInstaller
                            label: "CachyOS PackageInstaller"
                            onClicked: Backend.launchApp("package-installer")
                        }
                        HelloButton {
                            visible: Backend.hasKernelManager
                            label: "CachyOS Kernel Manager"
                            onClicked: Backend.launchApp("kernel-manager")
                        }
                    }
                }
            }
        }
    }
}
