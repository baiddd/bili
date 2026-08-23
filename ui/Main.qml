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
            id: screenLoader
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

    // Walks up from `item` (root.activeFocusItem) looking for the nearest
    // ancestor exposing GridView's or ListView's real navigation API.
    //
    // moveFocus() below used to probe root.activeFocusItem itself for these
    // methods, on the assumption that a GridView/ListView holds activeFocus
    // on the view itself while it's being navigated. That assumption is
    // empirically false (confirmed via live isolation testing, see Task 9's
    // review and this task's own re-verification below):
    // root.activeFocusItem actually resolves to the currently-highlighted
    // delegate (e.g. GameListScreen.qml's gameDelegate Rectangle), not the
    // GridView/ListView itself. Neither moveCurrentIndexUp/Down/Left/Right
    // (GridView) nor incrementCurrentIndex/decrementCurrentIndex (ListView)
    // exist on a plain delegate, so the old direct probe never matched and
    // moveFocus() silently fell through to the KeyNavigation lookup (also
    // undefined for a delegate with no KeyNavigation set), doing nothing --
    // this is why gamepad-driven navigation was a complete no-op. Walking
    // the .parent chain finds the enclosing view regardless of whether
    // activeFocusItem happens to be the view or one of its descendants,
    // without needing to know exactly why the delegate ends up holding
    // activeFocus in this Qt version.
    function findEnclosingView(item) {
        var current = item
        while (current) {
            if (typeof current.moveCurrentIndexUp === "function") return current
            if (typeof current.incrementCurrentIndex === "function" && typeof current.decrementCurrentIndex === "function") return current
            current = current.parent
        }
        return null
    }

    function moveFocus(direction) {
        var item = root.activeFocusItem
        if (!item) return

        // GridView/ListView-like items (e.g. GameListScreen's GridView,
        // SettingsScreen's romList ListView) are the ones gamepad input
        // needs to drive, since the KeyNavigation.<dir> lookup below (meant
        // for Button-to-Button navigation) doesn't apply to them. Gamepad
        // input only ever reaches QML through this function (see
        // GamepadBridge -> InputManager::navigateUp/Down/etc, which bypass
        // QML's Keys system entirely), so find the enclosing view -- whether
        // it's activeFocusItem itself or an ancestor of it -- and move its
        // currentIndex using its real Qt Quick API:
        //
        // GridView exposes moveCurrentIndexUp()/Down()/Left()/Right() as
        // public slots (2D navigation) -- confirmed in
        // QtQuick/private/qquickgridview_p.h.
        var view = findEnclosingView(item)
        if (view) {
            var methodName = "moveCurrentIndex" + direction.charAt(0).toUpperCase() + direction.slice(1)
            if (typeof view[methodName] === "function") {
                view[methodName]()
                return
            }

            // ListView only exposes incrementCurrentIndex()/decrementCurrentIndex()
            // (1D navigation along its orientation axis) -- confirmed in
            // QtQuick/private/qquicklistview_p.h; it has no moveCurrentIndex*
            // methods at all, so the check above never matches it.
            if (typeof view.incrementCurrentIndex === "function" && typeof view.decrementCurrentIndex === "function") {
                var isVertical = view.orientation === undefined || view.orientation === Qt.Vertical
                if ((isVertical && direction === "down") || (!isVertical && direction === "right")) {
                    view.incrementCurrentIndex()
                } else if ((isVertical && direction === "up") || (!isVertical && direction === "left")) {
                    view.decrementCurrentIndex()
                }
                return
            }
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
            if (EmulatorProvider.isGameMenuOpen()) return
            if (ScreenManager.currentScreen === "GameList") {
                ScreenManager.push("MainMenu")
            } else {
                ScreenManager.pop()
            }
        }
        // Whole-branch review fix (Fix 5): guarding only on isGameMenuOpen()
        // here was fragile -- it happened to work only because this
        // Connections block (Main.qml's own) is created before
        // InGameMenuOverlay.qml's, so Qt invokes THIS handler first, before
        // the overlay's own handler has a chance to clear the flag. Reverse
        // that load order (e.g. a future change to app/main.cpp's init
        // sequence) and Accept-while-menu-open would: the overlay's handler
        // quits the game and clears the flag first, THEN this guard sees
        // `false` and falls through to GameList's own accept logic,
        // relaunching the very game that was just quit. isGameRunning() does
        // not depend on that ordering at all, and additionally blocks this
        // screen-level logic during actual gameplay even with the menu
        // closed (closing a second, pre-existing gap that Fix 6's focus
        // handling made newly reachable by keyboard right after a resume).
        function onNavigateUp() {
            if (EmulatorProvider.isGameRunning() || EmulatorProvider.isGameMenuOpen()) return
            moveFocus("up")
        }
        function onNavigateDown() {
            if (EmulatorProvider.isGameRunning() || EmulatorProvider.isGameMenuOpen()) return
            moveFocus("down")
        }
        function onNavigateLeft() {
            if (EmulatorProvider.isGameRunning() || EmulatorProvider.isGameMenuOpen()) return
            moveFocus("left")
        }
        function onNavigateRight() {
            if (EmulatorProvider.isGameRunning() || EmulatorProvider.isGameMenuOpen()) return
            moveFocus("right")
        }
        function onAccept() {
            if (EmulatorProvider.isGameRunning() || EmulatorProvider.isGameMenuOpen()) return
            if (root.activeFocusItem && typeof root.activeFocusItem.clicked === "function") {
                root.activeFocusItem.clicked()
            } else if (root.activeFocusItem
                       && typeof root.activeFocusItem.romPath === "string"
                       && ScreenManager.currentScreen === "GameList") {
                // accept on a selected game in GameListScreen's GridView
                // opens GameDetailsScreen. Contrary to this branch's original
                // assumption (GridView keeps activeFocus on the view itself),
                // manual verification found that root.activeFocusItem is
                // actually the currently-highlighted delegate Rectangle
                // itself (GameListScreen.qml's gameDelegate) -- GridView
                // never held activeFocus at runtime, so the old
                // `typeof root.activeFocusItem.moveCurrentIndexUp === "function"`
                // check (a GridView-only method) never matched and this
                // whole branch was silently dead code. Detecting via
                // `romPath` (a property gameDelegate itself now exposes)
                // and reading the game's data directly off
                // root.activeFocusItem (not a nonexistent `.currentItem`)
                // fixes this. Scoped to GameList specifically (rather than
                // any romPath-shaped item) via the currentScreen check, so
                // a future screen with a same-shaped delegate isn't silently
                // swept into this same behavior.
                var romPath = root.activeFocusItem.romPath
                var system = root.activeFocusItem.system
                var title = root.activeFocusItem.gameTitle

                // selectedGame* is set in both branches below (direct launch
                // and GameDetails fallback) so that if the user backs out of
                // GameDetailsScreen afterward, or the direct launch fails,
                // GameDetailsScreen/other UI reading these properties still
                // reflects this game rather than stale data from a previous
                // selection.
                ScreenManager.selectedGameRomPath = romPath
                ScreenManager.selectedGameSystem = system
                ScreenManager.selectedGameTitle = title

                if (EmulatorProvider.isCoreInstalled(system)) {
                    EmulatorProvider.launchGame(romPath, system)
                } else {
                    ScreenManager.push("GameDetails")
                }
            } else if (ScreenManager.currentScreen === "EmulatorManager"
                       && screenLoader.item
                       && typeof screenLoader.item.triggerCurrentRowAction === "function") {
                // EmulatorManagerScreen's row list is a ListView (see
                // moveFocus()'s comment above) that holds keyboard/gamepad
                // focus itself, not any individual row's Install/Uninstall
                // Button - so the root.activeFocusItem.clicked() branch above
                // never fires for it. Delegate to the loaded screen's own
                // triggerCurrentRowAction(), which performs the same
                // install/uninstall logic as the highlighted row's Button
                // would, driven by the ListView's currentIndex instead.
                screenLoader.item.triggerCurrentRowAction()
            }
        }
    }

    Connections {
        target: ScreenManager
        function onCurrentScreenChanged() {
            if (ScreenManager.currentScreen === "GameList") {
                LibraryScanner.startScan(RomSourcesStore.sources())
            }
        }
    }
}
