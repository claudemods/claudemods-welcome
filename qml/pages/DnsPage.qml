import QtQuick
import ClaudeMods.Hello
import QtQuick.Controls
import QtQuick.Layouts

// DNS settings for NetworkManager connections (plain / DoT / DoH / DoQ).
Item {
    id: page

    readonly property var servers: Backend.dnsServers()
    readonly property var connections: Backend.nmConnections()
    readonly property int customIndex: servers.length
    readonly property int dhcpIndex: servers.length + 1

    // current selection (source of truth; the combo boxes mirror it)
    property int serverIndex: dhcpIndex
    property string dnsType: "plain"
    property string connection: ""

    readonly property bool isCustom: serverIndex === customIndex
    readonly property bool isDhcp: serverIndex === dhcpIndex
    readonly property bool isPreset: !isCustom && !isDhcp

    readonly property var serverNames: {
        const dependsOnLocale = Backend.locale
        const names = []
        for (let i = 0; i < servers.length; ++i)
            names.push(servers[i].name)
        names.push(I18n.t("custom-dns"))
        names.push(I18n.t("dhcp-automatic"))
        return names
    }

    // Which transports the selected server offers (plain is always available)
    readonly property var typeOptions: {
        const dependsOnLocale = Backend.locale
        const opts = [{ id: "plain", text: I18n.t("dns-type-plain") }]
        if (isDhcp)
            return opts
        const s = isPreset ? servers[serverIndex] : { dot: true, doh: true, doq: true }
        if (s.dot) opts.push({ id: "dot", text: I18n.t("dns-type-dot") })
        if (s.doh) opts.push({ id: "doh", text: I18n.t("dns-type-doh") })
        if (s.doq) opts.push({ id: "doq", text: I18n.t("dns-type-doq") })
        return opts
    }

    function typeIndexOf(id) {
        for (let i = 0; i < typeOptions.length; ++i)
            if (typeOptions[i].id === id)
                return i
        return 0
    }

    function chooseServer(index) {
        serverIndex = index
        if (index === dhcpIndex || index === customIndex)
            dnsType = "plain"
        else
            dnsType = servers[index].dot ? "dot" : "plain"
        serverBox.currentIndex = index
    }

    function loadConnection(name) {
        connection = name
        const st = Backend.dnsState(name)
        serverIndex = st.serverIndex
        dnsType = st.type
        ipv4Field.text = st.ipv4
        ipv6Field.text = st.ipv6
        dotField.text = st.dotHost
        dohField.text = st.dohUrl
        doqField.text = st.doqEndpoint
        serverBox.currentIndex = st.serverIndex
    }

    Component.onCompleted: {
        // preselect the active connection
        const active = Backend.activeConnection()
        const idx = connections.indexOf(active)
        if (idx >= 0) {
            connectionBox.currentIndex = idx
            loadConnection(active)
        } else if (connections.length > 0) {
            connectionBox.currentIndex = 0
            loadConnection(connections[0])
        }
    }

    Connections {
        target: Backend

        function onLatencyResult(ms) {
            latencyLabel.text = ms >= 0 ? ms.toFixed(2) + " ms" : I18n.t("latency-timeout")
            latencyLabel.font.bold = ms >= 0
            testButton.enabled = true
        }

        function onBestServerResult(index, ms) {
            if (index >= 0) {
                page.chooseServer(index)
                latencyLabel.text = ms.toFixed(2) + " ms"
                latencyLabel.font.bold = true
            } else {
                latencyLabel.text = I18n.t("latency-no-result")
                latencyLabel.font.bold = false
            }
            bestButton.enabled = true
        }

        function onLocaleChanged() {
            // the model is rebuilt on a language change; restore the selection afterwards
            Qt.callLater(() => serverBox.currentIndex = page.serverIndex)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        BackBar { target: "tweaks" }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: Math.min(scroll.availableWidth, 640)
                x: (scroll.availableWidth - width) / 2
                spacing: 8

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: I18n.t("dns-settings")
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: I18n.t("select-connection") }
                    ComboBox {
                        id: connectionBox
                        Layout.fillWidth: true
                        model: page.connections
                        onActivated: index => page.loadConnection(page.connections[index])
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: I18n.t("select-dns-server") }
                    ComboBox {
                        id: serverBox
                        Layout.fillWidth: true
                        model: page.serverNames
                        currentIndex: page.serverIndex
                        onActivated: index => page.chooseServer(index)
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    visible: page.isPreset
                    textFormat: Text.RichText
                    font.pointSize: Qt.application.font.pointSize * 0.85
                    text: page.isPreset
                          ? page.servers[page.serverIndex].region + " - <a href=\"" + page.servers[page.serverIndex].homepage
                            + "\">" + page.servers[page.serverIndex].homepage + "</a>"
                          : ""
                    onLinkActivated: link => Backend.openUrl(link)
                }

                // --- custom server fields -------------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: page.isCustom
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: I18n.t("custom-dns-ip", { version: "IPv4" }) }
                        TextField {
                            id: ipv4Field
                            Layout.fillWidth: true
                            placeholderText: "e.g. 1.1.1.1,1.0.0.1"
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: I18n.t("custom-dns-ip", { version: "IPv6" }) }
                        TextField {
                            id: ipv6Field
                            Layout.fillWidth: true
                            placeholderText: "e.g. 2606:4700:4700::1111"
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: I18n.t("custom-dns-dot-hostname") }
                        TextField {
                            id: dotField
                            Layout.fillWidth: true
                            placeholderText: "e.g. dns.example.com"
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: I18n.t("custom-dns-doh-url") }
                        TextField {
                            id: dohField
                            Layout.fillWidth: true
                            placeholderText: "e.g. https://dns.example.com/dns-query"
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: I18n.t("custom-dns-doq-endpoint") }
                        TextField {
                            id: doqField
                            Layout.fillWidth: true
                            placeholderText: "e.g. quic:dns.example.com"
                        }
                    }
                }

                // --- latency ---------------------------------------------------------------
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8

                    Button {
                        id: testButton
                        text: I18n.t("test-latency")
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: I18n.t("test-latency-tooltip")
                        onClicked: {
                            if (page.isCustom && ipv4Field.text.trim() === "")
                                return
                            if (page.isDhcp)
                                return
                            latencyLabel.text = I18n.t("latency-testing")
                            latencyLabel.font.bold = false
                            enabled = false
                            Backend.testLatency({
                                custom: page.isCustom,
                                ipv4: ipv4Field.text,
                                serverIndex: page.serverIndex,
                                type: page.dnsType
                            })
                        }
                    }
                    Button {
                        id: bestButton
                        visible: page.isPreset
                        text: I18n.t("best-server")
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: I18n.t("best-server-tooltip")
                        onClicked: {
                            latencyLabel.text = I18n.t("latency-testing")
                            latencyLabel.font.bold = false
                            enabled = false
                            Backend.bestServer()
                        }
                    }
                    Label { id: latencyLabel }
                }

                // --- transport -------------------------------------------------------------
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8

                    Label { text: I18n.t("dns-type-label") }
                    ComboBox {
                        id: typeBox
                        enabled: !page.isDhcp
                        model: page.typeOptions
                        textRole: "text"
                        currentIndex: page.typeIndexOf(page.dnsType)
                        onActivated: index => page.dnsType = page.typeOptions[index].id

                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: I18n.t("dns-type-tooltip")
                    }
                }

                // --- apply / reset -----------------------------------------------------------
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        Layout.fillWidth: true
                        text: I18n.t("reset")
                        onClicked: {
                            page.chooseServer(page.dhcpIndex)
                            Backend.resetDns(page.connection)
                        }
                    }
                    Button {
                        Layout.fillWidth: true
                        text: I18n.t("apply")
                        highlighted: true
                        onClicked: {
                            if (page.isDhcp) {
                                Backend.applyDns({ connection: page.connection, serverIndex: page.serverIndex })
                                return
                            }
                            Backend.applyDns({
                                connection: page.connection,
                                serverIndex: page.serverIndex,
                                type: page.dnsType,
                                ipv4: ipv4Field.text,
                                ipv6: ipv6Field.text,
                                dotHost: dotField.text,
                                dohUrl: dohField.text,
                                doqEndpoint: doqField.text
                            })
                        }
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    textFormat: Text.RichText
                    horizontalAlignment: Text.AlignHCenter
                    font.pointSize: Qt.application.font.pointSize * 0.85
                    text: I18n.t("dns-check-hint", {
                                     dnscheck_url: "<a href=\"https://dnscheck.tools\">dnscheck.tools</a>"
                                 })
                    onLinkActivated: link => Backend.openUrl(link)
                }
            }
        }
    }
}
