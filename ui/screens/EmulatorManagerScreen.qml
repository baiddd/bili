import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground

    Component.onCompleted: refreshList()

    function refreshList() {
        var rows = [{ rowId: "retroarch", label: "RetroArch" }]
        var knownSystems = EmulatorProvider.knownSystems()
        for (var i = 0; i < knownSystems.length; i++) {
            rows.push({ rowId: "core:" + knownSystems[i], label: "Core " + knownSystems[i] })
        }
        listModel.clear()
        for (var j = 0; j < rows.length; j++) {
            listModel.append({
                rowId: rows[j].rowId,
                label: rows[j].label,
                progressFraction: 0,
                progressVisible: false
            })
        }
    }

    // Finds the row matching a "retroarch"/"core:<system>" target tag (the
    // same tags EmulatorProvider's install/uninstall signals use) and
    // returns its index, or -1 if not found - used by the Connections
    // handlers below to update that one row's own progress state.
    function rowIndexForTarget(target) {
        for (var i = 0; i < listModel.count; i++) {
            if (listModel.get(i).rowId === target) return i
        }
        return -1
    }

    // Mirrors the delegate Button's own onClicked below, but driven by
    // emulatorList.currentIndex instead of a specific delegate's own `model`
    // context - called from Main.qml's onAccept() because this ListView (not
    // any row's Button) holds keyboard/gamepad focus, so pressing Enter/
    // gamepad-A on a highlighted row would otherwise do nothing (see
    // Main.qml's EmulatorManager-specific onAccept() branch).
    function triggerCurrentRowAction() {
        if (emulatorList.currentIndex < 0) return
        var row = listModel.get(emulatorList.currentIndex)
        var installed = row.rowId === "retroarch" ? EmulatorProvider.isRetroArchInstalled() : EmulatorProvider.isCoreInstalled(row.rowId.substring(5))
        if (row.rowId === "retroarch") {
            installed ? EmulatorProvider.uninstallRetroArch() : EmulatorProvider.installRetroArch()
        } else {
            var system = row.rowId.substring(5)
            installed ? EmulatorProvider.uninstallCore(system) : EmulatorProvider.installCore(system)
        }
    }

    ListModel { id: listModel }

    Column {
        anchors.fill: parent
        anchors.margins: Theme.spacingUnit * 2
        spacing: Theme.spacingUnit

        Text { id: titleText; text: "Émulateurs"; color: Theme.colorText; font.pixelSize: Theme.fontSizeTitle }

        ListView {
            id: emulatorList
            width: parent.width
            // Reserve exactly the space titleText and statusText actually need
            // (plus the Column's own two inter-item spacing gaps) rather than a
            // flat guess -- a fixed "Theme.spacingUnit * 6" guess left less room
            // than titleText's real height (~fontSizeTitle * 1.2) plus two
            // spacing gaps needed, pushing statusText below the window's own
            // bottom edge where it was silently clipped and never visible.
            height: parent.height - titleText.height - statusText.height - Theme.spacingUnit * 2
            focus: true
            keyNavigationEnabled: true
            highlightFollowsCurrentItem: true
            highlight: Rectangle { color: Theme.focusBorderColor; opacity: 0.5; border.color: Theme.focusBorderColor; border.width: Theme.focusBorderWidth }
            Component.onCompleted: forceActiveFocus()
            model: listModel
            delegate: Row {
                spacing: Theme.spacingUnit
                property bool installed: model.rowId === "retroarch" ? EmulatorProvider.isRetroArchInstalled() : EmulatorProvider.isCoreInstalled(model.rowId.substring(5))
                Text {
                    text: model.label + (installed ? " (installé)" : "")
                    color: Theme.colorText
                    font.pixelSize: Theme.fontSizeBody
                    width: 220
                }
                ProgressBar {
                    width: 150
                    visible: model.progressVisible
                    from: 0; to: 1
                    value: model.progressFraction
                }
                Button {
                    text: installed ? "Désinstaller" : "Installer"
                    onClicked: {
                        if (model.rowId === "retroarch") {
                            installed ? EmulatorProvider.uninstallRetroArch() : EmulatorProvider.installRetroArch()
                        } else {
                            var system = model.rowId.substring(5)
                            installed ? EmulatorProvider.uninstallCore(system) : EmulatorProvider.installCore(system)
                        }
                    }
                }
            }
        }

        // Fixed height (rather than the implicit height Text would otherwise
        // report as 0 while text is "") so emulatorList's height above stays
        // stable instead of jumping every time a status message appears/
        // disappears, and so this line always reserves real, visible space.
        Text {
            id: statusText
            height: font.pixelSize + Theme.spacingUnit
            color: Theme.colorAccent
            font.pixelSize: Theme.fontSizeBody
        }
    }

    Connections {
        target: EmulatorProvider
        function onInstallProgress(target, bytesReceived, bytesTotal) {
            var index = rowIndexForTarget(target)
            if (index === -1) return
            listModel.setProperty(index, "progressVisible", true)
            listModel.setProperty(index, "progressFraction", bytesReceived / Math.max(bytesTotal, 1))
        }
        function onInstallFinished(target) {
            statusText.text = target + " installé."
            refreshList()
        }
        function onInstallFailed(target, errorString) {
            var index = rowIndexForTarget(target)
            if (index !== -1) listModel.setProperty(index, "progressVisible", false)
            statusText.text = "Échec (" + target + ") : " + errorString
        }
        function onUninstallFinished(target) {
            statusText.text = target + " désinstallé."
            refreshList()
        }
        function onUninstallFailed(target, errorString) {
            statusText.text = "Échec (" + target + ") : " + errorString
        }
    }
}
