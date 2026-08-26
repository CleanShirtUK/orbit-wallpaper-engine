import Quickshell
import Quickshell.Io
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

Item {
    id: root

    property var themeData: ({
        uiFont: "Sans Serif",
        colors: ({
            text: "#c0caf5",
            text_muted: "#9aa5ce",
            surface: "#24283b",
            surface_elevated: "#2c3148",
            surface_selected: "#333954",
            accent: "#7aa2f7",
            accent_foreground: "#16161e",
            border: "#3d4355",
            error: "#f7768e",
            success: "#9ece6a",
            warning: "#e0af68"
        })
    })
    property var previewStyle: ({ corner_radius: 10 })
    readonly property int standaloneSmallCornerRadius: 8
    property string controlPath: Quickshell.env("HOME") + "/.local/bin/orbit-wallpaper-control"
    property string wallpaperServiceStatus: "unknown"
    property string wallpaperReadiness: "unknown"
    property string wallpaperAnimation: "unknown"
    property int wallpaperSurfaceCount: 0
    property int wallpaperOutputCount: 0
    property bool wallpaperRestartRequired: false
    property string wallpaperLastError: ""
    property bool showModeTabs: false

    function textColor() { return themeData.colors.text || "#c0caf5" }
    function mutedColor() { return themeData.colors.text_muted || "#9aa5ce" }
    function surfaceColor() { return themeData.colors.surface || "#24283b" }
    function selectedColor() { return themeData.colors.surface_selected || "#333954" }
    function accentColor() { return themeData.colors.accent || "#7aa2f7" }
    function previewColor(key, fallback) { return themeData.colors[key] || fallback }

    component WallpaperButton: Button {
        id: control

        // Same public contract and visual behaviour as OrbitButton.qml,
        // but self-contained so Orbit Wallpaper Engine has no Orbit Theme.qml
        // runtime dependency.
        property var themeData: root.themeData
        property bool destructive: false
        property bool subtle: false
        property bool compact: false

        implicitHeight: compact ? 32 : 36
        leftPadding: 14
        rightPadding: 14
        topPadding: 0
        bottomPadding: 0

        contentItem: Text {
            text: control.text
            color: !control.enabled ? Qt.alpha(control.textColor(), 0.45)
                : control.destructive ? control.errorColor()
                : control.highlighted ? control.foregroundColor()
                : control.textColor()
            font.family: control.themeData && control.themeData.uiFont
                ? control.themeData.uiFont
                : "JetBrains Mono"
            font.pixelSize: control.compact ? 11 : 12
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: control.themeData && control.themeData.smallCornerRadius !== undefined
                ? control.themeData.smallCornerRadius
                : 8
            color: !control.enabled ? control.surfaceColor(0.35)
                : control.pressed ? control.surfaceColor(0.95)
                : control.hovered ? control.surfaceColor(0.82)
                : control.highlighted ? control.accentColor()
                : control.subtle ? "transparent"
                : control.surfaceColor(0.68)
            border.width: control.activeFocus ? 2 : (control.subtle ? 0 : 1)
            border.color: control.activeFocus
                ? control.accentColor()
                : control.borderColor()
        }

        function colors() {
            return control.themeData && control.themeData.colors
                ? control.themeData.colors
                : root.themeData.colors
        }

        function textColor() {
            return colors().text || "#c0caf5"
        }

        function foregroundColor() {
            return colors().accent_foreground || "#16161e"
        }

        function accentColor() {
            return colors().accent || "#7aa2f7"
        }

        function errorColor() {
            return colors().error || "#f7768e"
        }

        function surfaceColor(alpha) {
            return Qt.alpha(colors().surface || "#24283b", alpha)
        }

        function borderColor() {
            return colors().border || "#3d4355"
        }
    }


    component WallpaperSlider: Slider {
        id: control

        implicitHeight: 24

        background: Rectangle {
            x: control.leftPadding
            y: control.topPadding + control.availableHeight / 2 - height / 2
            width: control.availableWidth
            height: 4
            radius: 2
            color: Qt.alpha(root.themeData.colors.border || "#3d4355", 0.95)

            Rectangle {
                width: control.visualPosition * parent.width
                height: parent.height
                radius: parent.radius
                color: root.accentColor()
            }
        }

        handle: Rectangle {
            x: control.leftPadding
                + control.visualPosition * (control.availableWidth - width)
            y: control.topPadding + control.availableHeight / 2 - height / 2
            width: 14
            height: 14
            radius: 7
            color: control.pressed
                ? Qt.lighter(root.themeData.colors.text || "#c0caf5", 1.08)
                : (root.themeData.colors.text || "#c0caf5")
            border.width: 1
            border.color: Qt.alpha(root.themeData.colors.border || "#3d4355", 0.8)
        }
    }

    component WallpaperCheckBox: CheckBox {
        id: control

        spacing: 8
        implicitHeight: 28

        indicator: Rectangle {
            implicitWidth: 18
            implicitHeight: 18
            x: 0
            y: parent.height / 2 - height / 2
            radius: 5
            color: control.checked
                ? root.accentColor()
                : Qt.alpha(root.surfaceColor(), 0.72)
            border.width: 1
            border.color: control.activeFocus
                ? root.accentColor()
                : (root.themeData.colors.border || "#3d4355")

            Text {
                anchors.centerIn: parent
                visible: control.checked
                text: "✓"
                color: root.themeData.colors.accent_foreground || "#16161e"
                font.family: root.themeData.uiFont
                font.pixelSize: 12
                font.bold: true
            }
        }

        contentItem: Text {
            leftPadding: control.indicator.width + control.spacing
            text: control.text
            color: control.enabled
                ? root.textColor()
                : Qt.alpha(root.textColor(), 0.45)
            font.family: root.themeData.uiFont
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
        }
    }

    component WallpaperComboBox: ComboBox {
        id: control

        implicitHeight: 36
        leftPadding: 12
        rightPadding: 34

        contentItem: Text {
            leftPadding: 0
            rightPadding: 0
            text: control.displayText
            color: root.textColor()
            font.family: root.themeData.uiFont
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        indicator: Text {
            x: control.width - width - 12
            y: control.height / 2 - height / 2
            text: "⌄"
            color: root.mutedColor()
            font.family: root.themeData.uiFont
            font.pixelSize: 14
        }

        background: Rectangle {
            radius: root.themeData.smallCornerRadius !== undefined
                ? root.themeData.smallCornerRadius : 8
            color: control.pressed
                ? Qt.alpha(root.surfaceColor(), 0.95)
                : control.hovered
                    ? Qt.alpha(root.surfaceColor(), 0.82)
                    : Qt.alpha(root.surfaceColor(), 0.68)
            border.width: control.activeFocus ? 2 : 1
            border.color: control.activeFocus
                ? root.accentColor()
                : (root.themeData.colors.border || "#3d4355")
        }

        popup: Popup {
            y: control.height + 4
            width: control.width
            implicitHeight: Math.min(contentItem.implicitHeight + 8, 320)
            padding: 4

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: control.popup.visible ? control.delegateModel : null
                currentIndex: control.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }

            background: Rectangle {
                radius: 8
                color: root.themeData.colors.surface_elevated
                    || root.themeData.colors.surface
                    || "#2c3148"
                border.width: 1
                border.color: root.themeData.colors.border || "#3d4355"
            }
        }

        delegate: ItemDelegate {
            width: control.width - 8
            highlighted: control.highlightedIndex === index

            contentItem: Text {
                text: modelData
                color: root.textColor()
                font.family: root.themeData.uiFont
                font.pixelSize: 11
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                radius: 6
                color: highlighted
                    ? Qt.alpha(root.accentColor(), 0.24)
                    : "transparent"
            }
        }
    }

    component WallpaperTextField: TextField {
        id: control

        implicitHeight: 36
        leftPadding: 12
        rightPadding: 12
        color: root.textColor()
        placeholderTextColor: root.mutedColor()
        selectionColor: root.accentColor()
        selectedTextColor: root.themeData.colors.accent_foreground || "#16161e"
        font.family: root.themeData.uiFont
        font.pixelSize: 11

        background: Rectangle {
            radius: root.themeData.smallCornerRadius !== undefined
                ? root.themeData.smallCornerRadius : 8
            color: Qt.alpha(root.surfaceColor(), 0.52)
            border.width: control.activeFocus ? 2 : 1
            border.color: control.activeFocus
                ? root.accentColor()
                : (root.themeData.colors.border || "#3d4355")
        }
    }

    component FastWheelHandler: WheelHandler {
        required property Flickable scroller
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

        onWheel: function(event) {
            var maximum = Math.max(0, scroller.contentHeight - scroller.height)
            if (maximum <= 0) {
                event.accepted = false
                return
            }

            var movement = 0
            if (event.angleDelta.y !== 0)
                movement = event.angleDelta.y > 0 ? 36 : -36
            else if (event.pixelDelta.y !== 0)
                movement = event.pixelDelta.y * 1.5

            if (movement === 0) {
                event.accepted = false
                return
            }

            scroller.cancelFlick()
            scroller.contentY = Math.max(
                0,
                Math.min(maximum, scroller.contentY - movement)
            )
            event.accepted = true
        }
    }

    function refreshRendererStatus() {
        if (!standaloneStatusProcess.running)
            standaloneStatusProcess.running = true
    }

    function restartRenderer() {
        if (!standaloneRestartProcess.running)
            standaloneRestartProcess.running = true
    }

    function apiError(value, fallback) {
        if (value && value.error) {
            if (value.error.message)
                return String(value.error.message)
            return String(value.error)
        }
        return fallback
    }

    function applyRendererStatus(value) {
        wallpaperServiceStatus = value.renderer && value.renderer.state
            ? value.renderer.state : "unknown"
        wallpaperReadiness = value.readiness || "unknown"
        wallpaperAnimation = value.animation || "unknown"
        wallpaperSurfaceCount = value.outputs
            && value.outputs.active_surfaces !== null
            && value.outputs.active_surfaces !== undefined
            ? Number(value.outputs.active_surfaces) : 0
        wallpaperOutputCount = value.outputs
            && value.outputs.count !== null
            && value.outputs.count !== undefined
            ? Number(value.outputs.count) : 0
        wallpaperRestartRequired = value.configuration
            ? Boolean(value.configuration.restart_required) : false
        wallpaperLastError = value.last_error && value.last_error.message
            ? String(value.last_error.message) : ""
    }

    Process {
        id: standaloneStatusProcess
        command: [root.controlPath, "status", "--json"]
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    var value = JSON.parse(text)
                    if (value.ok === false)
                        throw new Error(root.apiError(value, "Could not read renderer status"))
                    root.applyRendererStatus(value)
                } catch (error) {
                    root.wallpaperServiceStatus = "unknown"
                    root.wallpaperReadiness = "unknown"
                    root.wallpaperLastError = error.message
                }
            }
        }
    }

    Process {
        id: standaloneRestartProcess
        command: [root.controlPath, "restart"]
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    var value = JSON.parse(text)
                    if (value.ok === false)
                        throw new Error(root.apiError(value, "Renderer restart failed"))
                    if (value.status)
                        root.applyRendererStatus(value.status)
                    wallpaperRoot.wallpaperStatusText = "Renderer restarted."
                } catch (error) {
                    wallpaperRoot.wallpaperStatusText = error.message
                    root.wallpaperLastError = error.message
                }
            }
        }
        onExited: root.refreshRendererStatus()
    }

    Component.onCompleted: refreshRendererStatus()

        Item {
    id: wallpaperRoot
    anchors.fill: parent

        property var shaderFiles: ["wave.frag (default)"]
        property string selectedShader: ""
        property real introDuration: 4.5
        property real exitDuration: 1.0
        property real introPeakSpeed: 34.0
        property real introPeakStart: 0.05
        property real introPeakEnd: 0.08
        property real introRevealEnd: 0.22
        property real introDecay: 10.0
        property real paletteStrength: 0.72
        property real peakBrightness: 1.0

        function brightnessToSlider(value) {
            value = Math.max(0.0, Math.min(4.0, Number(value)))
            if (value <= 1.0)
                return value * 0.75
            return 0.75 + ((value - 1.0) / 3.0) * 0.25
        }

        function sliderToBrightness(position) {
            position = Math.max(0.0, Math.min(1.0, Number(position)))
            if (position <= 0.75)
                return position / 0.75
            return 1.0 + ((position - 0.75) / 0.25) * 3.0
        }

        function roundedBrightness(value) {
            if (value <= 1.0)
                return Math.round(value * 100.0) / 100.0
            return Math.round(value * 10.0) / 10.0
        }
        property real targetFps: 60.0
        property real renderScale: 1.0
        property real shaderSpeed: 1.0
        property bool resourceGovernor: true
        property real gpuPressureEnter: 75.0
        property real gpuPressureExit: 45.0
        property bool configLoaded: false
        property bool wallpaperDirty: false
        property string wallpaperStatusText: ""
        property bool shaderBrowserVisible: false
    property bool shaderCatalogLoading: false
        property string shaderCatalogError: ""
        property var shaderCatalog: []
        property var selectedCatalogShader: null
        property real shaderCatalogScrollY: 0
    property bool stageSucceeded: false
    property bool applySucceeded: false

        function parseConfig(text) {
            var payload = JSON.parse(String(text))
            if (payload.ok === false)
                throw new Error(root.apiError(payload, "Could not load wallpaper settings"))
            var settings = payload.settings || ({})

            function value(key, fallback) {
                var item = settings[key]
                return item && item.value !== null && item.value !== undefined
                    ? item.value : fallback
            }

            function numberValue(key, fallback) {
                var parsed = Number(value(key, fallback))
                return isFinite(parsed) ? parsed : fallback
            }

            function boolValue(key, fallback) {
                var parsed = String(value(key, "")).trim().toLowerCase()
                if (["1", "true", "yes", "on"].indexOf(parsed) >= 0)
                    return true
                if (["0", "false", "no", "off"].indexOf(parsed) >= 0)
                    return false
                return fallback
            }

            introDuration = numberValue("ORBIT_WALLPAPER_INTRO_DURATION", 4.5)
            exitDuration = numberValue("ORBIT_WALLPAPER_EXIT_DURATION", 1.0)
            introPeakSpeed = numberValue("ORBIT_WALLPAPER_INTRO_PEAK_SPEED", 34.0)
            introPeakStart = numberValue("ORBIT_WALLPAPER_INTRO_PEAK_START", 0.05)
            introPeakEnd = numberValue("ORBIT_WALLPAPER_INTRO_PEAK_END", 0.08)
            introRevealEnd = numberValue("ORBIT_WALLPAPER_INTRO_REVEAL_END", 0.22)
            introDecay = numberValue("ORBIT_WALLPAPER_INTRO_DECAY", 10.0)
            paletteStrength = numberValue("ORBIT_WALLPAPER_PALETTE_STRENGTH", 0.72)
            peakBrightness = numberValue("ORBIT_WALLPAPER_PEAK_BRIGHTNESS", 1.0)
            targetFps = numberValue("ORBIT_WALLPAPER_TARGET_FPS", 60.0)
            renderScale = numberValue("ORBIT_WALLPAPER_RENDER_SCALE", 1.0)
            shaderSpeed = numberValue("ORBIT_WALLPAPER_SPEED", 1.0)
            resourceGovernor = boolValue("ORBIT_WALLPAPER_RESOURCE_GOVERNOR", true)
            gpuPressureEnter = numberValue("ORBIT_WALLPAPER_GPU_PRESSURE_ENTER", 75.0)
            gpuPressureExit = numberValue("ORBIT_WALLPAPER_GPU_PRESSURE_EXIT", 45.0)
            selectedShader = value("ORBIT_WALLPAPER_SHADER", "")
            wallpaperRestartRequired = Boolean(payload.restart_required)
            wallpaperDirty = wallpaperRestartRequired
            configLoaded = true
        }

        function parseShaders(text) {
            var payload = JSON.parse(String(text))
            if (payload.ok === false)
                throw new Error(root.apiError(payload, "Could not list installed shaders"))
            var files = ["wave.frag (default)"]
            var installed = payload.shaders || []
            for (var i = 0; i < installed.length; ++i) {
                var file = String(installed[i])
                if (file && file !== "wave.frag" && files.indexOf(file) < 0)
                    files.push(file)
            }
            shaderFiles = files
        }

        function markDirty() {
            if (configLoaded)
                wallpaperDirty = true
        }

        function shaderIndex() {
            if (!selectedShader)
                return 0
            var index = shaderFiles.indexOf(selectedShader)
            return index < 0 ? 0 : index
        }

        function filteredCatalog(query) {
            var needle = String(query || "").trim().toLowerCase()
            if (!needle)
                return shaderCatalog
            var output = []
            for (var i = 0; i < shaderCatalog.length; ++i) {
                var item = shaderCatalog[i]
                var haystack = [
                    item.name || "",
                    item.author || "",
                    item.category || "",
                    item.license || "",
                    (item.tags || []).join(" ")
                ].join(" ").toLowerCase()
                if (haystack.indexOf(needle) >= 0)
                    output.push(item)
            }
            return output
        }

        function syncCatalogSelection(items) {
            if (!selectedCatalogShader)
                return
            for (var i = 0; i < items.length; ++i) {
                if (items[i].id === selectedCatalogShader.id) {
                    selectedCatalogShader = items[i]
                    return
                }
            }
            selectedCatalogShader = null
        }

        function openShaderBrowser() {
            shaderBrowserVisible = true
            shaderCatalogError = ""
            if (!shaderCatalog.length && !shaderCatalogProcess.running) {
                shaderCatalogLoading = true
                shaderCatalogProcess.command = [
                    root.controlPath,
                    "shader",
                    "catalogue",
                    "--json"
                ]
                shaderCatalogProcess.running = true
            }
        }

        function refreshShaderCatalog() {
            if (shaderCatalogProcess.running)
                return
            shaderCatalogScrollY = shaderCatalogList.contentY
            shaderCatalogLoading = true
            shaderCatalogError = ""
            shaderCatalogList.contentY = 0
            shaderCatalogProcess.command = [
                root.controlPath,
                "shader",
                "catalogue",
                "--refresh",
                "--json"
            ]
            shaderCatalogProcess.running = true
        }

        function installCatalogShader(item) {
            if (!item || shaderInstallProcess.running)
                return
            shaderCatalogScrollY = shaderCatalogList.contentY
            selectedCatalogShader = item
            shaderCatalogError = ""
            shaderInstallProcess.command = [
                root.controlPath,
                    "shader",
                    "apply",
                    String(item.id),
                    "--intro"
            ]
            shaderInstallProcess.running = true
        }

        function reloadConfig() {
            wallpaperStatusText = "Loading…"
            loadWallpaperConfig.running = true
            listWallpaperShaders.running = true
        }

        function replayIntro() {
            if (replayIntroProcess.running)
                return
            wallpaperStatusText = "Replaying intro…"
            replayIntroProcess.running = true
        }

        function saveConfig() {
            if (saveWallpaperConfig.running)
                return

            wallpaperRoot.stageSucceeded = false
            wallpaperStatusText = "Saving…"

            saveWallpaperConfig.command = [
                root.controlPath, "config", "set",
                "ORBIT_WALLPAPER_INTRO_DURATION", Number(introDuration).toFixed(3),
                "ORBIT_WALLPAPER_EXIT_DURATION", Number(exitDuration).toFixed(3),
                "ORBIT_WALLPAPER_INTRO_PEAK_SPEED", Number(introPeakSpeed).toFixed(3),
                "ORBIT_WALLPAPER_INTRO_PEAK_START", Number(introPeakStart).toFixed(3),
                "ORBIT_WALLPAPER_INTRO_PEAK_END", Number(introPeakEnd).toFixed(3),
                "ORBIT_WALLPAPER_INTRO_REVEAL_END", Number(introRevealEnd).toFixed(3),
                "ORBIT_WALLPAPER_INTRO_DECAY", Number(introDecay).toFixed(3),
                "ORBIT_WALLPAPER_PALETTE_STRENGTH", Number(paletteStrength).toFixed(3),
                "ORBIT_WALLPAPER_PEAK_BRIGHTNESS", Number(peakBrightness).toFixed(3),
                "ORBIT_WALLPAPER_TARGET_FPS", Number(targetFps).toFixed(1),
                "ORBIT_WALLPAPER_RENDER_SCALE", Number(renderScale).toFixed(2),
                "ORBIT_WALLPAPER_SPEED", Number(shaderSpeed).toFixed(2),
                "ORBIT_WALLPAPER_RESOURCE_GOVERNOR", resourceGovernor ? "1" : "0",
                "ORBIT_WALLPAPER_GPU_PRESSURE_ENTER", Number(gpuPressureEnter).toFixed(0),
                "ORBIT_WALLPAPER_GPU_PRESSURE_EXIT", Number(gpuPressureExit).toFixed(0),
                "ORBIT_WALLPAPER_SHADER", selectedShader || "wave.frag"
            ]

            saveWallpaperConfig.running = true
        }

        Component.onCompleted: reloadConfig()

        Process {
            id: shaderCatalogProcess
            running: false
            stdout: StdioCollector {
                onStreamFinished: {
                    wallpaperRoot.shaderCatalogLoading = false
                    try {
                        var value = JSON.parse(text)
                        if (value.ok === false)
                            throw new Error(root.apiError(value, "Could not load shader catalogue"))
                        wallpaperRoot.shaderCatalog = value.shaders || []
                        wallpaperRoot.shaderCatalogError = ""
                        wallpaperRoot.syncCatalogSelection(wallpaperRoot.shaderCatalog)
                        Qt.callLater(function() {
                            shaderCatalogList.contentY = Math.min(
                                wallpaperRoot.shaderCatalogScrollY,
                                Math.max(0, shaderCatalogList.contentHeight - shaderCatalogList.height)
                            )
                        })
                    } catch (error) {
                        wallpaperRoot.shaderCatalogError = error.message
                    }
                }
            }
            onExited: function(exitCode) {
                wallpaperRoot.shaderCatalogLoading = false
                if (exitCode !== 0 && !wallpaperRoot.shaderCatalogError)
                    wallpaperRoot.shaderCatalogError = "Could not load shader catalogue."
            }
        }

        Process {
            id: shaderInstallProcess
            running: false
            stdout: StdioCollector {
                onStreamFinished: {
                    try {
                        var value = JSON.parse(text)
                        if (value.ok === false)
                            throw new Error(root.apiError(value, "Shader apply failed"))
                        wallpaperRoot.wallpaperStatusText =
                            value.applied
                                ? "Installed and applied " + (value.shader && value.shader.name || "shader") + "."
                                : "Installed " + (value.name || "shader") + "."
                        wallpaperRoot.shaderCatalogError = ""
                        wallpaperRoot.reloadConfig()
                        wallpaperRoot.refreshShaderCatalog()
                    } catch (error) {
                        wallpaperRoot.shaderCatalogError = error.message
                    }
                }
            }
            onExited: function(exitCode) {
                if (exitCode !== 0) {
                    if (!wallpaperRoot.shaderCatalogError)
                        wallpaperRoot.shaderCatalogError = "Shader installation failed."
                    wallpaperRoot.reloadConfig()
                    wallpaperRoot.refreshShaderCatalog()
                }
            }
        }

        Process {
            id: loadWallpaperConfig
            command: [root.controlPath, "config", "get", "--json"]
            running: false
            stdout: StdioCollector {
                onStreamFinished: {
                    try {
                        wallpaperRoot.parseConfig(text)
                        wallpaperRoot.wallpaperStatusText = ""
                    } catch (error) {
                        wallpaperRoot.wallpaperStatusText = error.message
                        wallpaperRoot.wallpaperLastError = error.message
                    }
                }
            }
        }

        Process {
            id: listWallpaperShaders
            command: [root.controlPath, "shader", "list-installed", "--json"]
            running: false
            stdout: StdioCollector {
                onStreamFinished: {
                    try {
                        wallpaperRoot.parseShaders(text)
                    } catch (error) {
                        wallpaperRoot.wallpaperStatusText = error.message
                        wallpaperRoot.wallpaperLastError = error.message
                    }
                }
            }
        }

        Process {
            id: saveWallpaperConfig
            running: false
            stdout: StdioCollector {
                onStreamFinished: {
                    try {
                        var value = JSON.parse(text)
                        if (value.ok === false)
                            throw new Error(root.apiError(value, "Could not stage wallpaper settings"))
                        wallpaperRoot.stageSucceeded = true
                    } catch (error) {
                        wallpaperRoot.wallpaperStatusText = error.message
                        wallpaperRoot.wallpaperLastError = error.message
                    }
                }
            }
            onExited: function(exitCode) {
                if (exitCode !== 0 || !wallpaperRoot.stageSucceeded) {
                    wallpaperRoot.wallpaperStatusText = "Could not save wallpaper settings."
                    return
                }
                wallpaperRoot.wallpaperStatusText = "Applying wallpaper settings…"
                wallpaperRoot.applySucceeded = false
                applyWallpaperProcess.running = true
            }
        }

        Process {
            id: applyWallpaperProcess
            command: [root.controlPath, "config", "apply"]
            running: false
            stdout: StdioCollector {
                onStreamFinished: {
                    try {
                        var value = JSON.parse(text)
                        if (value.ok === false)
                            throw new Error(root.apiError(value, "Wallpaper settings could not be applied"))
                        wallpaperRoot.applySucceeded = true
                    } catch (error) {
                        wallpaperRoot.applySucceeded = false
                        wallpaperRoot.wallpaperStatusText = error.message
                        wallpaperRoot.wallpaperLastError = error.message
                    }
                }
            }
            onExited: function(exitCode) {
                if (exitCode !== 0 || !wallpaperRoot.applySucceeded) {
                    if (!wallpaperRoot.wallpaperStatusText)
                        wallpaperRoot.wallpaperStatusText = "Wallpaper settings could not be applied."
                    wallpaperRoot.reloadConfig()
                    root.refreshRendererStatus()
                    return
                }
                wallpaperRoot.wallpaperDirty = false
                wallpaperRoot.wallpaperRestartRequired = false
                wallpaperRoot.wallpaperStatusText = "Wallpaper settings applied with intro."
                wallpaperRoot.reloadConfig()
                root.refreshRendererStatus()
            }
        }

        Process {
            id: replayIntroProcess
            command: [root.controlPath, "intro"]
            running: false
            stdout: StdioCollector {
                onStreamFinished: {
                    try {
                        var value = JSON.parse(text)
                        if (value.ok === false)
                            throw new Error(root.apiError(value, "Could not replay intro"))
                    } catch (error) {
                        wallpaperRoot.wallpaperStatusText = error.message
                    }
                }
            }
            onExited: function(exitCode) {
                wallpaperRoot.wallpaperStatusText = exitCode === 0
                    ? "Intro replayed."
                    : "Could not replay intro."
                root.refreshRendererStatus()
            }
        }

        Flickable {
            id: settingsScroller
            anchors.fill: parent
            clip: true
            contentWidth: width
            contentHeight: wallpaperColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            FastWheelHandler { scroller: settingsScroller }

        Column {
            id: wallpaperColumn
            width: parent.width
            spacing: 10

            Row {
                visible: root.showModeTabs
                                width: parent.width
                                spacing: 10

                                WallpaperButton {
                                    width: (parent.width - 20) / 3
                                    text: "Shader"
                                    highlighted: wallpaperRoot.wallpaperMode === "shader"
                                    onClicked: wallpaperRoot.wallpaperMode = "shader"
                                }

                                WallpaperButton {
                                    width: (parent.width - 20) / 3
                                    text: "Wallpaper"
                                    highlighted: wallpaperRoot.wallpaperMode === "wallpaper"
                                    onClicked: {
                                        wallpaperRoot.wallpaperMode = "wallpaper"
                                        wallpaperRoot.wallpaperStatusText = "Wallpaper mode is not connected yet."
                                    }
                                }

                                WallpaperButton {
                                    width: (parent.width - 20) / 3
                                    text: "Static colour"
                                    highlighted: wallpaperRoot.wallpaperMode === "colour"
                                    onClicked: {
                                        wallpaperRoot.wallpaperMode = "colour"
                                        wallpaperRoot.wallpaperStatusText = "Static colour mode is not connected yet."
                                    }
                                }
                            }

            

            Item {
                width: parent.width
                height: 130

                Column {
                    anchors.fill: parent
                    spacing: 9

                    Row {
                        width: parent.width
                        spacing: 10

                        Text {
                            text: "Shader"
                            width: 112
                            color: textColor()
                            font.family: themeData.uiFont
                            font.pixelSize: 11
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        WallpaperComboBox {
                            width: parent.width - 112 - shaderBrowseButton.implicitWidth - 20
                            model: wallpaperRoot.shaderFiles
                            currentIndex: wallpaperRoot.shaderIndex()
                            onActivated: {
                                wallpaperRoot.selectedShader = currentIndex === 0 ? "" : currentText
                                wallpaperRoot.markDirty()
                            }
                        }

                        WallpaperButton {
                            id: shaderBrowseButton
                            themeData: root.themeData
                            compact: true
                            text: "Browse shaders…"
                            highlighted: true
                            onClicked: wallpaperRoot.openShaderBrowser()
                        }
                    }

                    Row {
                        width: parent.width
                        height: 32
                        spacing: 20

                        Row {
                            width: (parent.width - 20) / 2
                            height: parent.height
                            spacing: 8

                            Text {
                                text: "Palette strength"
                                width: 104
                                color: mutedColor()
                                font.family: themeData.uiFont
                                font.pixelSize: 10
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            WallpaperSlider {
                                width: parent.width - 104 - 44 - 16
                                from: 0.0
                                to: 1.0
                                stepSize: 0.01
                                value: wallpaperRoot.paletteStrength
                                anchors.verticalCenter: parent.verticalCenter
                                onMoved: {
                                    wallpaperRoot.paletteStrength = Math.round(value * 100) / 100
                                    wallpaperRoot.markDirty()
                                }
                            }

                            Text {
                                width: 44
                                text: Number(wallpaperRoot.paletteStrength).toFixed(2)
                                color: textColor()
                                font.family: "JetBrains Mono"
                                font.pixelSize: 9
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        Row {
                            width: (parent.width - 20) / 2
                            height: parent.height
                            spacing: 8

                            Text {
                                text: "Peak brightness"
                                width: 104
                                color: mutedColor()
                                font.family: themeData.uiFont
                                font.pixelSize: 10
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            WallpaperSlider {
                                width: parent.width - 104 - 44 - 16
                                from: 0.0
                                to: 1.0
                                stepSize: 0.005
                                value: wallpaperRoot.brightnessToSlider(wallpaperRoot.peakBrightness)
                                anchors.verticalCenter: parent.verticalCenter
                                onMoved: {
                                    wallpaperRoot.peakBrightness =
                                        wallpaperRoot.roundedBrightness(
                                            wallpaperRoot.sliderToBrightness(value))
                                    wallpaperRoot.markDirty()
                                }
                            }

                            Text {
                                width: 44
                                text: Number(wallpaperRoot.peakBrightness).toFixed(2) + "×"
                                color: textColor()
                                font.family: "JetBrains Mono"
                                font.pixelSize: 9
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    Text {
                        text: "Installed shaders are loaded from ~/.config/orbit-wallpaper-engine/shaders."
                        color: mutedColor()
                        font.family: themeData.uiFont
                        font.pixelSize: 9
                        width: parent.width
                        elide: Text.ElideRight
                    }
                }
            }

            Item {
                width: parent.width
                height: 116

                Column {
                    anchors.fill: parent
                    spacing: 7

                    Row {
                        width: parent.width
                        height: 22

                        Text {
                            text: "Performance"
                            color: textColor()
                            font.family: themeData.uiFont
                            font.pixelSize: 11
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Item {
                            width: Math.max(1, parent.width - 230)
                            height: 1
                        }

                        WallpaperCheckBox {
                            text: "Resource governor"
                            checked: wallpaperRoot.resourceGovernor
                            anchors.verticalCenter: parent.verticalCenter
                            onToggled: {
                                wallpaperRoot.resourceGovernor = checked
                                wallpaperRoot.markDirty()
                            }
                        }
                    }

                    // Primary performance controls form one balanced row.
                    Grid {
                        id: primaryPerformanceGrid
                        width: parent.width
                        columns: 3
                        columnSpacing: 14

                        Repeater {
                            model: [
                                ["Speed", "shaderSpeed", 0.0, 4.0, 0.05, "×", 2],
                                ["Target FPS", "targetFps", 10.0, 120.0, 1.0, " fps", 0],
                                ["Render scale", "renderScale", 0.25, 1.0, 0.05, "×", 2]
                            ]

                            delegate: Item {
                                required property var modelData
                                width: (primaryPerformanceGrid.width
                                    - primaryPerformanceGrid.columnSpacing * 2) / 3
                                height: 36

                                Text {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    text: modelData[0]
                                    color: mutedColor()
                                    font.family: themeData.uiFont
                                    font.pixelSize: 9
                                }

                                Text {
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    text: Number(wallpaperRoot[modelData[1]])
                                        .toFixed(Number(modelData[6])) + modelData[5]
                                    color: textColor()
                                    font.family: "JetBrains Mono"
                                    font.pixelSize: 9
                                }

                                WallpaperSlider {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 18
                                    from: Number(modelData[2])
                                    to: Number(modelData[3])
                                    stepSize: Number(modelData[4])
                                    value: Number(wallpaperRoot[modelData[1]])
                                    onMoved: {
                                        wallpaperRoot[modelData[1]] =
                                            Math.round(value / stepSize) * stepSize
                                        wallpaperRoot.markDirty()
                                    }
                                }
                            }
                        }
                    }

                    // Governor hysteresis stays compact and visually secondary.
                    Row {
                        width: parent.width
                        height: 36
                        spacing: 14
                        opacity: wallpaperRoot.resourceGovernor ? 1.0 : 0.45

                        Item {
                            width: (parent.width - 14) / 2
                            height: parent.height

                            Text {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                text: "GPU freeze"
                                color: mutedColor()
                                font.family: themeData.uiFont
                                font.pixelSize: 9
                            }

                            Text {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                text: Number(wallpaperRoot.gpuPressureEnter).toFixed(0) + "%"
                                color: textColor()
                                font.family: "JetBrains Mono"
                                font.pixelSize: 9
                            }

                            WallpaperSlider {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 18
                                from: 25
                                to: 100
                                stepSize: 1
                                value: wallpaperRoot.gpuPressureEnter
                                enabled: wallpaperRoot.resourceGovernor
                                onMoved: {
                                    wallpaperRoot.gpuPressureEnter = Math.round(value)
                                    if (wallpaperRoot.gpuPressureExit
                                            > wallpaperRoot.gpuPressureEnter)
                                        wallpaperRoot.gpuPressureExit =
                                            wallpaperRoot.gpuPressureEnter
                                    wallpaperRoot.markDirty()
                                }
                            }
                        }

                        Item {
                            width: (parent.width - 14) / 2
                            height: parent.height

                            Text {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                text: "GPU resume"
                                color: mutedColor()
                                font.family: themeData.uiFont
                                font.pixelSize: 9
                            }

                            Text {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                text: Number(wallpaperRoot.gpuPressureExit).toFixed(0) + "%"
                                color: textColor()
                                font.family: "JetBrains Mono"
                                font.pixelSize: 9
                            }

                            WallpaperSlider {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 18
                                from: 0
                                to: 95
                                stepSize: 1
                                value: wallpaperRoot.gpuPressureExit
                                enabled: wallpaperRoot.resourceGovernor
                                onMoved: {
                                    wallpaperRoot.gpuPressureExit = Math.round(value)
                                    if (wallpaperRoot.gpuPressureExit
                                            > wallpaperRoot.gpuPressureEnter)
                                        wallpaperRoot.gpuPressureEnter =
                                            wallpaperRoot.gpuPressureExit
                                    wallpaperRoot.markDirty()
                                }
                            }
                        }
                    }
                }
            }

            Item {
                width: parent.width
                height: 158

                Column {
                    anchors.fill: parent
                    spacing: 8

                    Row {
                        width: parent.width

                        Column {
                            width: parent.width - 120
                            spacing: 2

                            Text {
                                text: "Startup animation"
                                color: textColor()
                                font.family: themeData.uiFont
                                font.pixelSize: 11
                                font.bold: true
                            }

                            Text {
                                text: "Timing and motion used when Orbit reveals the wallpaper."
                                color: mutedColor()
                                font.family: themeData.uiFont
                                font.pixelSize: 9
                            }
                        }

                        WallpaperButton {
                            themeData: root.themeData
                            compact: true
                            subtle: true
                            text: "Reset values"
                            onClicked: {
                                wallpaperRoot.introDuration = 4.5
                                wallpaperRoot.exitDuration = 1.0
                                wallpaperRoot.introPeakSpeed = 34.0
                                wallpaperRoot.introPeakStart = 0.05
                                wallpaperRoot.introPeakEnd = 0.08
                                wallpaperRoot.introRevealEnd = 0.22
                                wallpaperRoot.introDecay = 10.0
                                wallpaperRoot.markDirty()
                            }
                        }
                    }

                    Grid {
                        id: animationGrid
                        width: parent.width
                        columns: 3
                        columnSpacing: 14
                        rowSpacing: 4

                        Repeater {
                            model: [
                                ["Duration", "introDuration", 0.1, 15.0, 0.1, " s"],
                                ["Exit duration", "exitDuration", 0.1, 10.0, 0.1, " s"],
                                ["Peak speed", "introPeakSpeed", 0.01, 100.0, 0.25, "×"],
                                ["Peak start", "introPeakStart", 0.0, 0.99, 0.01, ""],
                                ["Peak end", "introPeakEnd", 0.01, 1.0, 0.01, ""],
                                ["Reveal end", "introRevealEnd", 0.01, 1.0, 0.01, ""],
                                ["Decay", "introDecay", 0.01, 30.0, 0.1, ""]
                            ]

                            delegate: Item {
                                required property var modelData
                                width: (animationGrid.width
                                    - animationGrid.columnSpacing * 2) / 3
                                height: 38

                                Text {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    text: modelData[0]
                                    color: mutedColor()
                                    font.family: themeData.uiFont
                                    font.pixelSize: 9
                                }

                                Text {
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    text: Number(wallpaperRoot[modelData[1]]).toFixed(2) + modelData[5]
                                    color: textColor()
                                    font.family: "JetBrains Mono"
                                    font.pixelSize: 9
                                }

                                WallpaperSlider {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 20
                                    from: Number(modelData[2])
                                    to: Number(modelData[3])
                                    stepSize: Number(modelData[4])
                                    value: Number(wallpaperRoot[modelData[1]])
                                    onMoved: {
                                        var stepped = Math.round(value / stepSize) * stepSize
                                        wallpaperRoot[modelData[1]] = stepped
                                        wallpaperRoot.markDirty()
                                    }
                                }
                            }
                        }
                    }
                }
            }


            // Compact renderer status bar. The page title/description already live
            // in the Settings shell, so don't repeat them here.
            Item {
                width: parent.width
                height: statusColumn.implicitHeight

                Column {
                    id: statusColumn
                    anchors.fill: parent
                    spacing: 10

                    Column {
                        width: parent.width
                        height: 30
                        spacing: 3

                        Text {
                            text: "Shader renderer"
                            color: accentColor()
                            font.family: themeData.uiFont
                            font.pixelSize: 11
                            font.bold: true
                        }

                        Text {
                            text: (wallpaperRoot.selectedShader || "wave.frag")
                                + "  •  " + root.wallpaperServiceStatus
                                + "  •  " + root.wallpaperReadiness
                                + "  •  " + root.wallpaperSurfaceCount
                                + "/" + root.wallpaperOutputCount + " surfaces"
                                + (root.wallpaperRestartRequired ? "  •  restart required" : "")
                                + (wallpaperRoot.wallpaperStatusText
                                    ? "  •  " + wallpaperRoot.wallpaperStatusText
                                    : root.wallpaperLastError ? "  •  " + root.wallpaperLastError : "")
                            color: root.wallpaperServiceStatus === "active"
                                ? (themeData.colors.success || "#9ece6a")
                                : mutedColor()
                            font.family: "JetBrains Mono"
                            font.pixelSize: 9
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            width: parent.width
                        }
                    }

                    Flow {
                        width: parent.width
                        spacing: 10

                        WallpaperButton {
                            id: replayIntroButton
                            themeData: root.themeData
                            compact: true
                            text: "Replay intro"
                            enabled: root.wallpaperServiceStatus === "active"
                                && root.wallpaperReadiness === "ready"
                            onClicked: wallpaperRoot.replayIntro()
                        }

                        WallpaperButton {
                            id: applyWallpaperButton
                            themeData: root.themeData
                            compact: true
                            highlighted: true
                            text: "Apply"
                            enabled: wallpaperRoot.wallpaperDirty
                            onClicked: wallpaperRoot.saveConfig()
                        }

                        WallpaperButton {
                            id: restartServiceButton
                            themeData: root.themeData
                            compact: true
                            text: root.wallpaperServiceStatus === "active"
                                ? "Restart Renderer"
                                : "Start Renderer"
                            highlighted: root.wallpaperServiceStatus !== "active"
                            onClicked: root.restartRenderer()
                        }
                    }

                }
            }
    }
        }

        Rectangle {
            id: shaderBrowserOverlay
            anchors.fill: parent
            z: 100
            visible: wallpaperRoot.shaderBrowserVisible
            color: Qt.rgba(0.0, 0.0, 0.0, 0.55)

            MouseArea {
                anchors.fill: parent
                onClicked: {}
            }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 24, 680)
                height: Math.min(parent.height - 24, 500)
                radius: Number(root.previewStyle.corner_radius) + 2
                color: previewColor("window_background", "#1a1b26")
                border.color: previewColor("border", "#3d4355")
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Item {
                        width: parent.width
                        height: 34

                        Text {
                            anchors.left: parent.left
                            anchors.right: refreshCatalogButton.left
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Shader library"
                            color: textColor()
                            font.family: themeData.uiFont
                            font.pixelSize: 13
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            id: closeCatalogButton
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 34
                            height: 34
                            radius: width / 2
                            color: closeCatalogMouse.containsMouse
                                ? Qt.alpha(themeData.colors.error || "#f7768e", 0.48)
                                : Qt.alpha(themeData.colors.error || "#f7768e", 0.28)
                            border.width: 0

                            Text {
                                anchors.centerIn: parent
                                text: "×"
                                color: textColor()
                                font.family: themeData.uiFont
                                font.pixelSize: 14
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            MouseArea {
                                id: closeCatalogMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: wallpaperRoot.shaderBrowserVisible = false
                            }

                            Behavior on color {
                                ColorAnimation { duration: 100 }
                            }
                        }

                        Rectangle {
                            id: refreshCatalogButton
                            anchors.right: closeCatalogButton.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            width: 34
                            height: 34
                            radius: width / 2
                            enabled: !wallpaperRoot.shaderCatalogLoading
                                && !shaderInstallProcess.running
                            opacity: enabled ? 1.0 : 0.45
                            color: refreshCatalogMouse.containsMouse
                                ? Qt.alpha(accentColor(), 0.36)
                                : Qt.alpha(accentColor(), 0.18)
                            border.width: 0

                            Text {
                                anchors.centerIn: parent
                                text: "↻"
                                color: textColor()
                                font.family: themeData.uiFont
                                font.pixelSize: 16
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            MouseArea {
                                id: refreshCatalogMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                enabled: parent.enabled
                                cursorShape: Qt.PointingHandCursor
                                onClicked: wallpaperRoot.refreshShaderCatalog()
                            }

                            Behavior on color {
                                ColorAnimation { duration: 100 }
                            }
                        }
                    }

                    WallpaperTextField {
                        id: shaderSearch
                        width: parent.width
                        placeholderText: "Search shaders, authors, categories or licences…"
                        onTextChanged: {
                            shaderCatalogList.contentY = 0
                            wallpaperRoot.shaderCatalogScrollY = 0
                            wallpaperRoot.syncCatalogSelection(
                                wallpaperRoot.filteredCatalog(text)
                            )
                        }
                    }

                    Row {
                        width: parent.width
                        height: parent.height - 94
                        spacing: 12

                        Rectangle {
                            width: 300
                            height: parent.height
                            radius: 8
                            color: Qt.alpha(surfaceColor(), 0.32)

                            ListView {
                                id: shaderCatalogList
                                    FastWheelHandler { scroller: shaderCatalogList }
                                anchors.fill: parent
                                anchors.margins: 5
                                clip: true
                                spacing: 3
                                model: wallpaperRoot.filteredCatalog(shaderSearch.text)

                                delegate: Rectangle {
                                    required property var modelData
                                    width: shaderCatalogList.width - 10
                                    height: 54
                                    radius: 6
                                    color: wallpaperRoot.selectedCatalogShader
                                        && wallpaperRoot.selectedCatalogShader.id === modelData.id
                                        ? selectedColor()
                                        : "transparent"

                                    Column {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        spacing: 2

                                        Text {
                                            width: parent.width
                                            text: modelData.name
                                            color: textColor()
                                            font.family: themeData.uiFont
                                            font.pixelSize: 10
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            width: parent.width
                                            text: (modelData.category || "Other")
                                                + "  •  "
                                                + (modelData.license_label || modelData.license || "Unknown licence")
                                                + (modelData.supported ? "" : "  •  Future support")
                                            color: modelData.license_status === "upstream-unverified"
                                                ? (themeData.colors.warning || "#e0af68")
                                                : (modelData.supported
                                                    ? mutedColor()
                                                    : (themeData.colors.warning || "#e0af68"))
                                            font.family: "JetBrains Mono"
                                            font.pixelSize: 8
                                            elide: Text.ElideRight
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        enabled: !shaderInstallProcess.running
                                        onClicked: wallpaperRoot.selectedCatalogShader = modelData
                                    }
                                }
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: wallpaperRoot.shaderCatalogLoading
                                text: "Loading shader library…"
                                color: mutedColor()
                                font.family: "JetBrains Mono"
                                font.pixelSize: 10
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !wallpaperRoot.shaderCatalogLoading
                                    && !wallpaperRoot.shaderCatalog.length
                                    && !wallpaperRoot.shaderCatalogError
                                text: "No permitted shaders found."
                                color: mutedColor()
                                font.family: "JetBrains Mono"
                                font.pixelSize: 9
                            }
                        }

                        Item {
                            id: shaderDetailPane
                            width: parent.width - 312
                            height: parent.height

                            Column {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 48
                                spacing: 8

                                Rectangle {
                                    width: parent.width
                                    height: 150
                                    radius: 8
                                    color: Qt.alpha(surfaceColor(), 0.32)
                                    clip: true

                                    Image {
                                        anchors.fill: parent
                                        source: wallpaperRoot.selectedCatalogShader
                                            ? wallpaperRoot.selectedCatalogShader.thumbnail_url || ""
                                            : ""
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        visible: source !== ""
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        visible: !wallpaperRoot.selectedCatalogShader
                                            || !wallpaperRoot.selectedCatalogShader.thumbnail_url
                                        text: wallpaperRoot.selectedCatalogShader
                                            ? "No preview available"
                                            : "Select a shader"
                                        color: mutedColor()
                                        font.family: "JetBrains Mono"
                                        font.pixelSize: 10
                                    }
                                }

                                Text {
                                    width: parent.width
                                    text: wallpaperRoot.selectedCatalogShader
                                        ? wallpaperRoot.selectedCatalogShader.name
                                        : "Shader details"
                                    color: textColor()
                                    font.family: themeData.uiFont
                                    font.pixelSize: 13
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    width: parent.width
                                    text: {
                                        var item = wallpaperRoot.selectedCatalogShader
                                        if (!item) return ""
                                        var author = item.author || "Unknown author"
                                        var licenseText = item.license_label || item.license || "Unknown licence"
                                        return author + "  •  " + licenseText
                                    }
                                    color: mutedColor()
                                    font.family: "JetBrains Mono"
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }

                                Text {
                                    width: parent.width
                                    text: {
                                        var item = wallpaperRoot.selectedCatalogShader
                                        if (!item) return "Choose a shader from the library."
                                        if (item.supported)
                                            return "Compatible with the current Orbit renderer."
                                        var needs = (item.requirements || []).join(", ")
                                        return "Requires future renderer support: " + needs
                                    }
                                    color: wallpaperRoot.selectedCatalogShader
                                        && wallpaperRoot.selectedCatalogShader.supported
                                        ? (themeData.colors.success || "#9ece6a")
                                        : (themeData.colors.warning || "#e0af68")
                                    font.family: themeData.uiFont
                                    font.pixelSize: 9
                                    wrapMode: Text.WordWrap
                                }

                                Text {
                                    width: parent.width
                                    visible: wallpaperRoot.selectedCatalogShader
                                        && wallpaperRoot.selectedCatalogShader.license_status === "upstream-unverified"
                                    text: "Upstream shader source does not declare a licence. Orbit permits it under the shader catalogue policy and keeps the status visible."
                                    color: themeData.colors.warning || "#e0af68"
                                    font.family: themeData.uiFont
                                    font.pixelSize: 8
                                    wrapMode: Text.WordWrap
                                }

                                Item { width: 1; height: 1 }

                                Text {
                                    width: parent.width
                                    visible: wallpaperRoot.shaderCatalogError !== ""
                                    text: wallpaperRoot.shaderCatalogError
                                    color: themeData.colors.error || "#f7768e"
                                    font.family: "JetBrains Mono"
                                    font.pixelSize: 9
                                    wrapMode: Text.WordWrap
                                }

                            }


                            WallpaperButton {
                                themeData: root.themeData
                                text: wallpaperRoot.selectedCatalogShader
                                    && wallpaperRoot.selectedCatalogShader.installed
                                    ? "Reinstall & Apply"
                                    : "Install & Apply"
                                highlighted: true
                                enabled: wallpaperRoot.selectedCatalogShader
                                    && wallpaperRoot.selectedCatalogShader.supported
                                    && !shaderInstallProcess.running
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                onClicked: wallpaperRoot.installCatalogShader(
                                    wallpaperRoot.selectedCatalogShader
                                )
                            }



    }
                    }
                }
            }
        }
    }
}
