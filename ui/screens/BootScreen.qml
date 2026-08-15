import QtQuick
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground
    Text {
        anchors.centerIn: parent
        text: "Bili"
        color: Theme.colorText
        font.pixelSize: Theme.fontSizeTitle + 8
    }
    Timer {
        interval: 500
        running: true
        onTriggered: {
            ScreenManager.push(RomSourcesStore.hasAnySource() ? "MainMenu" : "FirstLaunchSetup")
        }
    }
}
