pragma Singleton
import QtQuick

QtObject {
    // Classic Qt/SeisComp-like workbench colors.
    readonly property color background:  "#d9d9d9"
    readonly property color surface:     "#eeeeee"
    readonly property color surfaceAlt:  "#ffffff"
    readonly property color navBg:       "#cfcfcf"
    readonly property color accent:      "#2f7dbd"
    readonly property color border:      "#9a9a9a"

    // Text
    readonly property color textPrimary:   "#101010"
    readonly property color textSecondary: "#4f4f4f"

    // Status colors
    readonly property color colorNormal:   "#00C853"
    readonly property color colorWarning:  "#FFD600"
    readonly property color colorCritical: "#FF1744"
    readonly property color colorOffline:  "#616161"
    readonly property color colorService:  "#0057a8"
    readonly property color colorHigh:     "#ff8c00"

    // Chart series
    readonly property color seriesX: "#d7191c"
    readonly property color seriesY: "#2c7bb6"
    readonly property color seriesZ: "#1a9641"

    // Typography
    readonly property int fontSizeSmall:  10
    readonly property int fontSizeNormal: 12
    readonly property int fontSizeLarge:  14
    readonly property int fontSizeTitle:  18

    // Layout
    readonly property int navWidth:    180
    readonly property int radius:        2
    readonly property int radiusSmall:   1

    function statusColor(warningLevel) {
        if (warningLevel === 2) return colorCritical
        if (warningLevel === 1) return colorWarning
        return colorNormal
    }

    function severityColor(level) {
        if (level === 3) return colorCritical
        if (level === 2) return colorHigh
        if (level === 1) return colorWarning
        return colorNormal
    }
}
