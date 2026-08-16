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

        // GridView/ListView-like items (e.g. GameListScreen's GridView,
        // SettingsScreen's romList ListView) hold activeFocus on the view
        // itself rather than on an individual delegate, so the
        // KeyNavigation.<dir> lookup below (meant for Button-to-Button
        // navigation) never applies to them -- it silently no-ops for
        // gamepad input, which only ever reaches QML through this function
        // (see GamepadBridge -> InputManager::navigateUp/Down/etc, which
        // bypass QML's Keys system entirely). Move the view's currentIndex
        // instead, using its real Qt Quick API:
        //
        // GridView exposes moveCurrentIndexUp()/Down()/Left()/Right() as
        // public slots (2D navigation) -- confirmed in
        // QtQuick/private/qquickgridview_p.h.
        var methodName = "moveCurrentIndex" + direction.charAt(0).toUpperCase() + direction.slice(1)
        if (typeof item[methodName] === "function") {
            item[methodName]()
            return
        }

        // ListView only exposes incrementCurrentIndex()/decrementCurrentIndex()
        // (1D navigation along its orientation axis) -- confirmed in
        // QtQuick/private/qquicklistview_p.h; it has no moveCurrentIndex*
        // methods at all, so the check above never matches it.
        if (typeof item.incrementCurrentIndex === "function" && typeof item.decrementCurrentIndex === "function") {
            var isVertical = item.orientation === undefined || item.orientation === Qt.Vertical
            if ((isVertical && direction === "down") || (!isVertical && direction === "right")) {
                item.incrementCurrentIndex()
            } else if ((isVertical && direction === "up") || (!isVertical && direction === "left")) {
                item.decrementCurrentIndex()
            }
            return
        }

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
