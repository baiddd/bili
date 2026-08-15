import QtQuick
import QtQuick.Controls

ApplicationWindow {
    visible: true
    width: 1280
    height: 720
    title: "Bili"

    Item {
        anchors.fill: parent
        focus: true
        Keys.onPressed: (event) => InputManager.handleKeyPress(event.key)

        Loader {
            anchors.fill: parent
            source: "screens/" + ScreenManager.currentScreen + "Screen.qml"
        }
    }
}
