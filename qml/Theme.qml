pragma Singleton
import QtQuick

QtObject {
    // Classic Qt/SeisComp-like workbench colors.
    readonly property color background:  "#d6d6d6"
    readonly property color surface:     "#efefef"
    readonly property color surfaceAlt:  "#ffffff"
    readonly property color navBg:       "#c8c8c8"
    readonly property color accent:      "#3b6ea5"
    readonly property color border:      "#8f8f8f"

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
    readonly property int fontSizeSmall:  11
    readonly property int fontSizeNormal: 13
    readonly property int fontSizeLarge:  16
    readonly property int fontSizeTitle:  20

    // Layout
    readonly property int navWidth:    180
    readonly property int radius:        6
    readonly property int radiusSmall:   4

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
