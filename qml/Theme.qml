pragma Singleton
import QtQuick

QtObject {
    // Backgrounds
    readonly property color background:  "#151820"
    readonly property color surface:     "#1D222C"
    readonly property color surfaceAlt:  "#242B36"
    readonly property color navBg:       "#0F131A"
    readonly property color accent:      "#14324A"
    readonly property color border:      "#343C48"

    // Text
    readonly property color textPrimary:   "#E0E0E0"
    readonly property color textSecondary: "#A0A0A0"

    // Status colors
    readonly property color colorNormal:   "#00C853"
    readonly property color colorWarning:  "#FFD600"
    readonly property color colorCritical: "#FF1744"
    readonly property color colorOffline:  "#616161"
    readonly property color colorService:  "#00E5FF"
    readonly property color colorHigh:     "#FF6D00"

    // Chart series
    readonly property color seriesX: "#FF4081"
    readonly property color seriesY: "#00E5FF"
    readonly property color seriesZ: "#69F0AE"

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
