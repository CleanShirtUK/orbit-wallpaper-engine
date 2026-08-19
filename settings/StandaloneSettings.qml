import Quickshell
import QtQuick
import QtQuick.Window

Window {
    id: window
    width: 900
    height: 620
    visible: true
    title: "Orbit Wallpaper Engine"
    color: "#12131b"
    flags: Qt.Window

    Rectangle {
        anchors.fill: parent
        anchors.margins: 18
        radius: 12
        color: "#1a1b26"
        border.width: 1
        border.color: "#3d4355"

        Item {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            height: 50

            Column {
                anchors.left: parent.left
                anchors.right: closeButton.left
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    text: "Orbit Wallpaper Engine"
                    color: "#c0caf5"
                    font.family: "JetBrains Mono"
                    font.pixelSize: 18
                    font.bold: true
                }

                Text {
                    text: "Shader wallpaper configuration"
                    color: "#9aa5ce"
                    font.family: "JetBrains Mono"
                    font.pixelSize: 10
                }
            }

            Rectangle {
                id: closeButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 34
                height: 34
                radius: 17
                color: closeMouse.containsMouse ? "#7af7768e" : "#47f7768e"

                Text {
                    anchors.centerIn: parent
                    text: "×"
                    color: "#c0caf5"
                    font.family: "JetBrains Mono"
                    font.pixelSize: 14
                    font.bold: true
                }

                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: window.close()
                }
            }
        }

        WallpaperSettings {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.bottom: parent.bottom
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            anchors.bottomMargin: 16
        }
    }
}
