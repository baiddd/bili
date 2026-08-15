import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
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

    function moveFocus(direction) {
        var item = root.activeFocusItem
        if (!item) return
        var next
        if (direction === "up") next = item.KeyNavigation.up
        else if (direction === "down") next = item.KeyNavigation.down
        else if (direction === "left") next = item.KeyNavigation.left
        else if (direction === "right") next = item.KeyNavigation.right
        if (next) next.forceActiveFocus()
    }

    Connections {
        target: InputManager
        function onCancel() {
            if (ScreenManager.currentScreen === "GameList") {
                ScreenManager.push("MainMenu")
            } else {
                ScreenManager.pop()
            }
        }
        function onNavigateUp() { moveFocus("up") }
        function onNavigateDown() { moveFocus("down") }
        function onNavigateLeft() { moveFocus("left") }
        function onNavigateRight() { moveFocus("right") }
        function onAccept() {
            if (root.activeFocusItem && typeof root.activeFocusItem.clicked === "function") {
                root.activeFocusItem.clicked()
            }
        }
    }
}
