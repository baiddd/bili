pragma Singleton
import QtQuick

QtObject {
    readonly property color colorBackground: "#101014"
    readonly property color colorAccent: "#4f8cff"
    readonly property color colorText: "#ffffff"

    readonly property int spacingUnit: 8
    readonly property int fontSizeBody: 16
    readonly property int fontSizeTitle: 32

    readonly property color focusBorderColor: colorAccent
    readonly property int focusBorderWidth: 2
    readonly property int focusRadius: 4

    readonly property int animationDurationMs: 200
    readonly property int animationEasing: Easing.InOutQuad

    readonly property int compactBreakpointWidth: 900

    function isCompact(width) {
        return width < compactBreakpointWidth;
    }
}
