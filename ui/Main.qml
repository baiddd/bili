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

        Button {
            id: menuButton
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 8
            width: 40
            height: 40
            text: "☰"
            visible: ScreenManager.currentScreen !== "MainMenu"
            onClicked: {
                if (ScreenManager.currentScreen !== "MainMenu") {
                    ScreenManager.push("MainMenu")
                }
            }
        }
    }

    Connections {
        target: InputManager
        function onCancel() { ScreenManager.pop() }
    }
}
