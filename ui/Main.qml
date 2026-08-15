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
        Keys.onPressed: (event) => {
            InputManager.handleKeyPress(event.key)
            event.accepted = true
        }

        Loader {
            anchors.fill: parent
            source: "screens/" + ScreenManager.currentScreen + "Screen.qml"
        }
    }

    Connections {
        target: InputManager
        function onCancel() { ScreenManager.pop() }
    }
}
