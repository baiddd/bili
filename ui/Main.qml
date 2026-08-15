import QtQuick
import QtQuick.Controls

ApplicationWindow {
    visible: true
    width: 1280
    height: 720
    title: "Bili"

    Loader {
        anchors.fill: parent
        source: "screens/" + ScreenManager.currentScreen + "Screen.qml"
    }
}
