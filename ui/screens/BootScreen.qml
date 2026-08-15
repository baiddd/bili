import QtQuick

Rectangle {
    anchors.fill: parent
    color: "black"
    Text {
        anchors.centerIn: parent
        text: "Bili"
        color: "white"
        font.pixelSize: 40
    }
    Timer {
        interval: 500
        running: true
        onTriggered: {
            ScreenManager.push(RomSourcesStore.hasAnySource() ? "MainMenu" : "FirstLaunchSetup")
        }
    }
}
