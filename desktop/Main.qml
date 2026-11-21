import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window

    width: 1280
    height: 800
    minimumWidth: 1000
    minimumHeight: 650
    visible: true
    title: "SmartFileVault"

    color: "#F5F1E9"

    property string currentPage: "Dashboard"
    property url selectedFolderUrl: ""
    property url selectedRestoreFolderUrl: ""
    property string selectedArchiveId: ""
    property string selectedDeleteArchiveId: ""
    property string selectedDeleteArchiveName: ""

    FolderDialog {
        id: folderDialog
        title: "Select a folder to scan"

        onAccepted: {
            window.selectedFolderUrl = selectedFolder
        }
    }

    FolderDialog {
        id: restoreFolderDialog
        title: "Select restore destination folder"

        onAccepted: {
            window.selectedRestoreFolderUrl = selectedFolder
        }
    }

    Dialog {
        id: confirmRestoreDialog
        title: "Confirm restore"
        modal: true
        width: 490
        anchors.centerIn: parent
        padding: 20

        background: Rectangle {
            color: "#FFFCF7"
            border.color: "#CBD5CA"
            border.width: 1
            radius: 14
        }

        contentItem: ColumnLayout {
            spacing: 18

            Text {
                text: "Restore archived file?"
                color: "#252B29"
                font.pixelSize: 19
                font.bold: true
            }

            Text {
                text: "Restore the selected archived file to this folder?\n\n"
                      + window.selectedRestoreFolderUrl.toString()
                      + "\n\nIf a file with the same name already exists, SmartFileVault may use an alternative filename."
                color: "#59635D"
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 10

                Button {
                    text: "Cancel"
                    onClicked: confirmRestoreDialog.reject()

                    contentItem: Text {
                        text: parent.text
                        color: "#303B34"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: "#D7E0D6"
                        radius: 7
                        border.color: "#B8C7BB"

                    opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
}
                }

                Button {
                    text: "OK, Restore"
                    onClicked: confirmRestoreDialog.accept()

                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: "#536F5B"
                        radius: 7

                    opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
}
                }
            }
        }

        onAccepted: {
            dashboard.restoreSelectedArchive(
                window.selectedArchiveId,
                window.selectedRestoreFolderUrl
            )
        }
    }

    Dialog {
        id: confirmScanDialog
        title: "Confirm archive operation"
        modal: true
        width: 490
        anchors.centerIn: parent
        padding: 20

        background: Rectangle {
            color: "#FFFCF7"
            border.color: "#CBD5CA"
            border.width: 1
            radius: 14
        }

        contentItem: ColumnLayout {
            spacing: 18

            Text {
                text: "Review archive operation"
                color: "#252B29"
                font.pixelSize: 19
                font.bold: true
            }

            Text {
                text: "SmartFileVault will scan this folder:\n\n"
                      + window.selectedFolderUrl.toString()
                      + "\n\nFiles inactive for "
                      + inactivityDays.value
                      + " days or more may be archived and removed from their original location.\n\nContinue?"
                color: "#59635D"
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 10

                Button {
                    text: "Cancel"
                    onClicked: confirmScanDialog.reject()

                    contentItem: Text {
                        text: parent.text
                        color: "#303B34"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: "#D7E0D6"
                        radius: 7
                        border.color: "#B8C7BB"

                    opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
}
                }

                Button {
                    text: "OK, Continue"
                    onClicked: confirmScanDialog.accept()

                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: "#536F5B"
                        radius: 7

                    opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
}
                }
            }
        }

        onAccepted: {
            dashboard.startScan(
                window.selectedFolderUrl,
                inactivityDays.value
            )
        }
    }


    Dialog {
        id: confirmDeleteDialog
        title: "Confirm permanent deletion"
        modal: true
        width: 510
        anchors.centerIn: parent
        padding: 20

        background: Rectangle {
            color: "#FFFCF7"
            border.color: "#CBD5CA"
            border.width: 1
            radius: 14
        }

        contentItem: ColumnLayout {
            spacing: 16

            Text {
                text: "Delete archived file permanently?"
                color: "#252B29"
                font.pixelSize: 19
                font.bold: true
            }

            Text {
                text: "File: " + window.selectedDeleteArchiveName
                      + "\n\nThis will permanently delete the archived file and its original file (if it exists). This action cannot be undone."
                color: "#59635D"
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 10

                Button {
                    text: "Cancel"
                    onClicked: confirmDeleteDialog.reject()

            background: Rectangle {
                radius: 8
                color: !parent.enabled ? "#E4E8E2"
                       : parent.down ? "#405B47"
                       : parent.hovered ? "#6F8D73"
                       : "#536F5B"
                border.color: parent.hovered ? "#405B47" : "#B8C7BB"
                opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
            }
        }

                Button {
                    text: "Delete Permanently"
                    enabled: !dashboard.deleting
                    onClicked: confirmDeleteDialog.accept()

                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: "#A83D45"
                        radius: 7

                    opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
}
                }
            }
        }

        onAccepted: {
            dashboard.deleteSelectedArchive(window.selectedDeleteArchiveId)
        }
    }

    Component.onCompleted: dashboard.refresh()

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 225
            Layout.fillHeight: true
            color: "#DCE8DC"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 24

                ColumnLayout {
                    spacing: 5

                    Text {
                        text: "SmartFileVault"
                        color: "#252B29"
                        font.pixelSize: 21
                        font.bold: true
                    }

                    Text {
                        text: "SMART STORAGE MANAGER"
                        color: "#68756C"
                        font.pixelSize: 9
                        font.letterSpacing: 1.5
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#C5D3C6"
                }

                Text {
                    text: "WORKSPACE"
                    color: "#6C796E"
                    font.pixelSize: 10
                    font.bold: true
                }

                Repeater {
                    model: [
                        "Dashboard",
                        "Scan",
                        "Archives",
                        "Restore",
                        "Files",
                        "Settings"
                    ]

                    delegate: Button {
                        id: navButton

                        required property string modelData

                        Layout.fillWidth: true
                        height: 44
                        text: modelData
                        checkable: true
                        checked: window.currentPage === modelData

                        onClicked: window.currentPage = modelData

                        background: Rectangle {
                            radius: 9
                            color: navButton.checked
                                   ? "#A8C2AC"
                                   : navButton.hovered
                                     ? "#C9D9CB"
                                     : "transparent"

                    opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
}

                        contentItem: Text {
                            text: navButton.text
                            color: navButton.checked
                                   ? "#FFFFFF"
                                   : "#4F5F53"
                            font.pixelSize: 14
                            font.weight: navButton.checked
                                         ? Font.DemiBold
                                         : Font.Normal
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 14
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }

                Text {
                    text: "PHASE III  •  DESKTOP"
                    color: "#6C796E"
                    font.pixelSize: 10
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true

            gradient: Gradient {
                GradientStop { position: 0.0; color: "#F5F1E9" }
                GradientStop { position: 0.48; color: "#EFF3EC" }
                GradientStop { position: 1.0; color: "#EEF3F6" }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 34
                spacing: 26

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        spacing: 5

                        Text {
                            text: window.currentPage
                            color: "#252B29"
                            font.pixelSize: 28
                            font.bold: true
                        }

                        Text {
                            text: "Manage your storage, archives and files."
                            color: "#68756C"
                            font.pixelSize: 13
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        radius: 18
                        color: "#E2ECE3"
                        border.color: "#BFD0C1"
                        width: 105
                        height: 34

                        Text {
                            anchors.centerIn: parent
                            text: "LOCAL VAULT"
                            color: "#426C52"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#D9DED5"
                }

                ColumnLayout {
                    visible: window.currentPage === "Dashboard"
                    Layout.fillWidth: true
                    spacing: 18

                    Text {
                        text: "Storage overview"
                        color: "#2C342E"
                        font.pixelSize: 18
                        font.bold: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 14

                        Repeater {
                            model: [
                                { label: "Archived files", value: dashboard.archiveCount },
                                { label: "Space saved", value: dashboard.spaceSaved },
                                { label: "Original data", value: dashboard.originalData },
                                { label: "Free space", value: dashboard.freeSpace }
                            ]

                            delegate: Rectangle {
                                required property var modelData

                                Layout.fillWidth: true
                                Layout.preferredHeight: 125
                                radius: 13
                                color: modelData.label === "Archived files" ? "#DCEBDD"
                                     : modelData.label === "Space saved" ? "#B9D9BF"
                                     : modelData.label === "Original data" ? "#E5EBDD"
                                     : "#D2DFC9"
                                border.color: "#D7E0D6"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 18
                                    spacing: 12

                                    Text {
                                        text: modelData.label
                                        color: "#68756C"
                                        font.pixelSize: 12
                                    }

                                    Text {
                                        text: modelData.value
                                        color: "#252B29"
                                        font.pixelSize: 27
                                        font.bold: true
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        text: dashboard.statusMessage
                        color: "#68756C"
                        font.pixelSize: 13
                    }
                }

                Item {
                    visible: window.currentPage === "Archives"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: "Archived files"
                                color: "#252B29"
                                font.pixelSize: 21
                                font.bold: true
                            }

                            Item { Layout.fillWidth: true }

                            Button {
                                text: "Refresh"
                                enabled: !dashboard.scanning && !dashboard.restoring && !dashboard.deleting
                                onClicked: dashboard.refresh()

            background: Rectangle {
                radius: 8
                color: !parent.enabled ? "#E4E8E2"
                       : parent.down ? "#405B47"
                       : parent.hovered ? "#6F8D73"
                       : "#536F5B"
                border.color: parent.hovered ? "#405B47" : "#B8C7BB"
                opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
            }
        }
                        }

                        Text {
                            text: dashboard.archivesStatus
                            color: "#68756C"
                            font.pixelSize: 12
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            BusyIndicator {
                                running: dashboard.deleting
                                visible: dashboard.deleting
                                width: 24
                                height: 24
                            }

                            Text {
                                text: dashboard.deleteStatus
                                color: dashboard.deleting ? "#426C52" : "#59665D"
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                        }

                        ListView {
                            id: archiveList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 10
                            clip: true
                            model: dashboard.archives

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                            }

                            delegate: Rectangle {
                                required property var modelData

                                width: archiveList.width
                                height: 130
                                radius: 11
                                color: "#FFFCF7"
                                border.color: "#D7E0D6"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 6

                                    RowLayout {
                                        Layout.fillWidth: true

                                        Text {
                                            text: modelData.originalName
                                            color: "#252B29"
                                            font.pixelSize: 14
                                            font.bold: true
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            text: modelData.archivedAt
                                            color: "#68756C"
                                            font.pixelSize: 11
                                        }
                                    }

                                    Text {
                                        text: "Archive ID: " + modelData.archiveId
                                        color: "#68756C"
                                        font.pixelSize: 10
                                        Layout.fillWidth: true
                                        elide: Text.ElideMiddle
                                    }

                                    Text {
                                        text: "Original: " + modelData.originalSize
                                              + "    Compressed: " + modelData.compressedSize
                                              + "    Compression: " + modelData.compressionPercentage
                                        color: "#59665D"
                                        font.pixelSize: 11
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Item { Layout.fillWidth: true }

                                        Button {
                                            text: "Delete"
                                            enabled: !dashboard.deleting && !dashboard.scanning && !dashboard.restoring
                                            onClicked: {
                                                window.selectedDeleteArchiveId = modelData.archiveId
                                                window.selectedDeleteArchiveName = modelData.originalName
                                                confirmDeleteDialog.open()
                                            }

                                            contentItem: Text {
                                                text: parent.text
                                                color: "#FFFFFF"
                                                font.pixelSize: 11
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle {
                                                color: "#71343D"
                                                radius: 6
                                                border.color: "#A84B55"

                    opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    visible: window.currentPage === "Scan"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ScrollView {
                        anchors.fill: parent
                        clip: true
                        contentWidth: availableWidth

                        ColumnLayout {
                            width: parent.width
                            spacing: 22

                        Text {
                            text: "Scan and archive inactive files"
                            color: "#252B29"
                            font.pixelSize: 21
                            font.bold: true
                        }

                        Text {
                            text: "Choose a local folder and set how long a file must remain inactive before it becomes eligible for archiving."
                            color: "#68756C"
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 145
                            radius: 12
                            color: "#FFFCF7"
                            border.color: "#D7E0D6"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 18
                                spacing: 12

                                Text {
                                    text: "1. Select folder"
                                    color: "#2C342E"
                                    font.pixelSize: 15
                                    font.bold: true
                                }

                                RowLayout {
                                    Layout.fillWidth: true

                                    Text {
                                        text: window.selectedFolderUrl.toString().length > 0
                                              ? window.selectedFolderUrl.toString()
                                              : "No folder selected"
                                        color: "#68756C"
                                        font.pixelSize: 12
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }

                                    Button {
                                        text: "Choose Folder"
                                        enabled: !dashboard.scanning && !dashboard.restoring && !dashboard.deleting
                                        onClicked: folderDialog.open()

            background: Rectangle {
                radius: 8
                color: !parent.enabled ? "#E4E8E2"
                       : parent.down ? "#405B47"
                       : parent.hovered ? "#6F8D73"
                       : "#536F5B"
                border.color: parent.hovered ? "#405B47" : "#B8C7BB"
                opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
            }
        }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 125
                            radius: 12
                            color: "#FFFCF7"
                            border.color: "#D7E0D6"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 18
                                spacing: 10

                                Text {
                                    text: "2. Inactivity period"
                                    color: "#2C342E"
                                    font.pixelSize: 15
                                    font.bold: true
                                }

                                RowLayout {
                                    Text {
                                        text: "Archive files inactive for"
                                        color: "#68756C"
                                        font.pixelSize: 13
                                    }

                                    SpinBox {
                                        id: inactivityDays
                                        from: 1
                                        to: 3650
                                        value: dashboard.defaultInactivityDays
                                        editable: true
                                        enabled: !dashboard.scanning && !dashboard.restoring && !dashboard.deleting
                                                                            onValueModified: dashboard.setDefaultInactivityDays(value)

            background: Rectangle {
                radius: 7
                color: parent.activeFocus ? "#F1F6EF"
                       : parent.hovered ? "#F7F9F5"
                       : "#FFFCF7"
                border.width: parent.activeFocus ? 2 : parent.hovered ? 1.5 : 1
                border.color: parent.activeFocus ? "#536F5B"
                             : parent.hovered ? "#8AA58D"
                             : "#CBD5CA"
            }
        }

                                    Text {
                                        text: "days or more"
                                        color: "#68756C"
                                        font.pixelSize: 13
                                    }
                                }
                            }
                        }

                        Button {
                            text: dashboard.scanning
                                  ? "Archiving in progress..."
                                  : "Review and Start"
                            enabled: !dashboard.scanning
                                     && !dashboard.restoring
                                     && !dashboard.deleting
                                     && window.selectedFolderUrl.toString().length > 0
                            onClicked: confirmScanDialog.open()

            background: Rectangle {
                radius: 8
                color: !parent.enabled ? "#E4E8E2"
                       : parent.down ? "#405B47"
                       : parent.hovered ? "#6F8D73"
                       : "#536F5B"
                border.color: parent.hovered ? "#405B47" : "#B8C7BB"
                opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
            }
        }

                        RowLayout {
                            visible: dashboard.scanning
                            spacing: 12

                            BusyIndicator {
                                running: dashboard.scanning
                                visible: dashboard.scanning
                                width: 28
                                height: 28
                            }

                            Text {
                                text: dashboard.scanStatus
                                color: "#426C52"
                                font.pixelSize: 13
                            }
                        }

                        Text {
                            visible: !dashboard.scanning
                            text: dashboard.scanStatus
                            color: "#68756C"
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            visible: dashboard.scanSummary.length > 0
                            Layout.fillWidth: true
                            implicitHeight: summaryContent.implicitHeight + 36
                            Layout.preferredHeight: implicitHeight
                            radius: 12
                            color: "#FFFCF7"
                            border.color: "#D7E0D6"

                            ColumnLayout {
                                id: summaryContent
                                anchors.fill: parent
                                anchors.margins: 18
                                spacing: 10

                                Text {
                                    text: "Operation summary"
                                    color: "#252B29"
                                    font.pixelSize: 16
                                    font.bold: true
                                }

                                Text {
                                    text: dashboard.scanSummary
                                    color: "#59665D"
                                    font.pixelSize: 13
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }
                        }
                    }
                }

                Item {
                    visible: window.currentPage === "Restore"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ScrollView {
                        anchors.fill: parent
                        clip: true
                        contentWidth: availableWidth

                        ColumnLayout {
                            width: parent.width
                            spacing: 20

                            Text {
                                text: "Restore archived files"
                                color: "#252B29"
                                font.pixelSize: 21
                                font.bold: true
                            }

                            Text {
                                text: "Select an archived file and choose where you want to restore it."
                                color: "#68756C"
                                font.pixelSize: 13
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 155
                                radius: 12
                                color: "#FFFCF7"
                                border.color: "#D7E0D6"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 18
                                    spacing: 12

                                    Text {
                                        text: "1. Select archived file"
                                        color: "#2C342E"
                                        font.pixelSize: 15
                                        font.bold: true
                                    }

                                    ComboBox {
                                        id: restoreArchiveCombo
                                        Layout.fillWidth: true
                                        model: dashboard.archives
                                        textRole: "originalName"
                                        currentIndex: dashboard.archives.length > 0 ? 0 : -1
                                        enabled: !dashboard.restoring && !dashboard.scanning && !dashboard.deleting
                                        onCurrentIndexChanged: {
                                            if (currentIndex >= 0
                                                && currentIndex < dashboard.archives.length) {
                                                window.selectedArchiveId =
                                                    dashboard.archives[currentIndex].archiveId
                                            } else {
                                                window.selectedArchiveId = ""
                                            }
                                        }

            background: Rectangle {
                radius: 7
                color: parent.activeFocus ? "#F1F6EF"
                       : parent.hovered ? "#F7F9F5"
                       : "#FFFCF7"
                border.width: parent.activeFocus ? 2 : parent.hovered ? 1.5 : 1
                border.color: parent.activeFocus ? "#536F5B"
                             : parent.hovered ? "#8AA58D"
                             : "#CBD5CA"
            }
        }

                                    Text {
                                        text: dashboard.archives.length > 0
                                              ? dashboard.archives.length + " archived file(s) available"
                                              : dashboard.archivesStatus
                                        color: "#68756C"
                                        font.pixelSize: 11
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 145
                                radius: 12
                                color: "#FFFCF7"
                                border.color: "#D7E0D6"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 18
                                    spacing: 12

                                    Text {
                                        text: "2. Choose destination"
                                        color: "#2C342E"
                                        font.pixelSize: 15
                                        font.bold: true
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true

                                        Text {
                                            text: window.selectedRestoreFolderUrl.toString().length > 0
                                                  ? window.selectedRestoreFolderUrl.toString()
                                                  : "No destination selected"
                                            color: "#68756C"
                                            font.pixelSize: 12
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }

                                        Button {
                                            text: "Choose Folder"
                                            enabled: !dashboard.restoring && !dashboard.scanning && !dashboard.deleting
                                            onClicked: restoreFolderDialog.open()

            background: Rectangle {
                radius: 8
                color: !parent.enabled ? "#E4E8E2"
                       : parent.down ? "#405B47"
                       : parent.hovered ? "#6F8D73"
                       : "#536F5B"
                border.color: parent.hovered ? "#405B47" : "#B8C7BB"
                opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
            }
        }
                                    }
                                }
                            }

                            RowLayout {
                                spacing: 12

                                Button {
                                    text: dashboard.restoring
                                          ? "Restoring..."
                                          : "Restore File"
                                    enabled: !dashboard.restoring
                                             && !dashboard.scanning
                                             && !dashboard.deleting
                                             && window.selectedArchiveId.length > 0
                                             && window.selectedRestoreFolderUrl.toString().length > 0
                                    onClicked: confirmRestoreDialog.open()

            background: Rectangle {
                radius: 8
                color: !parent.enabled ? "#E4E8E2"
                       : parent.down ? "#405B47"
                       : parent.hovered ? "#6F8D73"
                       : "#536F5B"
                border.color: parent.hovered ? "#405B47" : "#B8C7BB"
                opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
            }
        }

                                Button {
                                    text: "Refresh Archives"
                                    enabled: !dashboard.restoring && !dashboard.scanning && !dashboard.deleting
                                    onClicked: dashboard.refresh()

            background: Rectangle {
                radius: 8
                color: !parent.enabled ? "#E4E8E2"
                       : parent.down ? "#405B47"
                       : parent.hovered ? "#6F8D73"
                       : "#536F5B"
                border.color: parent.hovered ? "#405B47" : "#B8C7BB"
                opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
            }
        }

                                BusyIndicator {
                                    running: dashboard.restoring
                                    visible: dashboard.restoring
                                    width: 28
                                    height: 28
                                }
                            }

                            Text {
                                text: dashboard.restoreStatus
                                color: dashboard.restoring ? "#426C52" : "#59665D"
                                font.pixelSize: 13
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }

                            Rectangle {
                                visible: dashboard.restoredPath.length > 0
                                Layout.fillWidth: true
                                implicitHeight: restoredContent.implicitHeight + 36
                                Layout.preferredHeight: implicitHeight
                                radius: 12
                                color: "#FFFCF7"
                                border.color: "#D7E0D6"

                                ColumnLayout {
                                    id: restoredContent
                                    anchors.fill: parent
                                    anchors.margins: 18
                                    spacing: 10

                                    Text {
                                        text: "Restored file location"
                                        color: "#426C52"
                                        font.pixelSize: 15
                                        font.bold: true
                                    }

                                    Text {
                                        text: dashboard.restoredPath
                                        color: "#59665D"
                                        font.pixelSize: 12
                                        wrapMode: Text.Wrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            Item {
                                Layout.fillHeight: true
                            }
                        }
                    }
                }

                Item {
                    visible: window.currentPage === "Files"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ScrollView {
                        anchors.fill: parent
                        clip: true
                        contentWidth: availableWidth

                        ColumnLayout {
                            width: parent.width
                            spacing: 18

                            Text {
                                text: "Scanned Files"
                                color: "#252B29"
                                font.pixelSize: 21
                                font.bold: true
                            }

                            Text {
                                text: "View files recorded in your latest scan report."
                                color: "#68756C"
                                font.pixelSize: 13
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Repeater {
                                    model: [
                                        { name: "Total Files", count: dashboard.totalScanFiles },
                                        { name: "Active", count: dashboard.activeScanFiles },
                                        { name: "Inactive", count: dashboard.inactiveScanFiles },
                                        { name: "Skipped", count: dashboard.skippedScanFiles }
                                    ]

                                    delegate: Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 82
                                        radius: 10
                                        color: "#FFFCF7"
                                        border.color: "#D7E0D6"

                                        ColumnLayout {
                                            anchors.centerIn: parent
                                            spacing: 5

                                            Text {
                                                text: modelData.name
                                                color: "#68756C"
                                                font.pixelSize: 11
                                                Layout.alignment: Qt.AlignHCenter
                                            }

                                            Text {
                                                text: modelData.count
                                                color: "#252B29"
                                                font.pixelSize: 22
                                                font.bold: true
                                                Layout.alignment: Qt.AlignHCenter
                                            }
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                TextField {
                                    id: fileSearch
                                    placeholderText: "Search file name..."
                                    Layout.fillWidth: true
                                    color: "#2C342E"
                                    placeholderTextColor: "#68756C"

                                    background: Rectangle {
                                        color: "#FFFCF7"
                                        radius: 8


                                        border.width: parent.activeFocus ? 2 : parent.hovered ? 1.5 : 1
                                        border.color: parent.activeFocus ? "#536F5B" : parent.hovered ? "#8AA58D" : "#CBD5CA"
}
                                }

                                ComboBox {
                                    id: fileStatusFilter
                                    model: ["All", "Active", "Inactive"]
                                    implicitWidth: 130

            background: Rectangle {
                radius: 7
                color: parent.activeFocus ? "#F1F6EF"
                       : parent.hovered ? "#F7F9F5"
                       : "#FFFCF7"
                border.width: parent.activeFocus ? 2 : parent.hovered ? 1.5 : 1
                border.color: parent.activeFocus ? "#536F5B"
                             : parent.hovered ? "#8AA58D"
                             : "#CBD5CA"
            }
        }

                                Button {
                                    text: "Refresh"
                                    enabled: !dashboard.scanning && !dashboard.restoring
                                    onClicked: dashboard.refresh()

            background: Rectangle {
                radius: 8
                color: !parent.enabled ? "#E4E8E2"
                       : parent.down ? "#405B47"
                       : parent.hovered ? "#6F8D73"
                       : "#536F5B"
                border.color: parent.hovered ? "#405B47" : "#B8C7BB"
                opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
            }
        }
                            }

                            Text {
                                text: dashboard.filesStatus
                                color: "#68756C"
                                font.pixelSize: 12
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: fileRows.implicitHeight + 24
                                Layout.preferredHeight: implicitHeight
                                radius: 10
                                color: "#FFFCF7"
                                border.color: "#D7E0D6"

                                ColumnLayout {
                                    id: fileRows
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 0

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        Layout.preferredHeight: 38

                                        Text {
                                            text: "FILE NAME"
                                            Layout.minimumWidth: 180
                                            Layout.preferredWidth: 180
                                            Layout.maximumWidth: 180
                                            color: "#68756C"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }

                                        Text {
                                            text: "TYPE"
                                            Layout.minimumWidth: 65
                                            Layout.preferredWidth: 65
                                            Layout.maximumWidth: 65
                                            color: "#68756C"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }

                                        Text {
                                            text: "SIZE"
                                            Layout.minimumWidth: 75
                                            Layout.preferredWidth: 75
                                            Layout.maximumWidth: 75
                                            color: "#68756C"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }

                                        Text {
                                            text: "LAST MODIFIED"
                                            Layout.fillWidth: true
                                            color: "#68756C"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }

                                        Text {
                                            text: "STATUS"
                                            Layout.minimumWidth: 75
                                            Layout.preferredWidth: 75
                                            Layout.maximumWidth: 75
                                            color: "#68756C"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    Repeater {
                                        model: dashboard.scanFiles

                                        delegate: Rectangle {
                                            property bool matchesSearch:
                                                modelData.fileName.toLowerCase()
                                                    .includes(fileSearch.text.toLowerCase())

                                            property bool matchesStatus:
                                                fileStatusFilter.currentText === "All"
                                                || modelData.status
                                                    === fileStatusFilter.currentText.toLowerCase()

                                            visible: matchesSearch && matchesStatus
                                            Layout.fillWidth: true
                                            Layout.minimumHeight: 0
                                            Layout.preferredHeight: visible ? 62 : 0
                                            Layout.maximumHeight: visible ? 62 : 0
                                            color: "transparent"

                                            RowLayout {
                                                anchors.fill: parent
                                                spacing: 8

                                                ColumnLayout {
                                            Layout.minimumWidth: 180
                                            Layout.preferredWidth: 180
                                            Layout.maximumWidth: 180
                                                    spacing: 3

                                                    Text {
                                                        text: modelData.fileName
                                                        color: "#2C342E"
                                                        font.pixelSize: 11
                                                        elide: Text.ElideMiddle
                                                        Layout.fillWidth: true
                                                    }

                                                    Text {
                                                        text: modelData.filePath
                                                        color: "#7C877E"
                                                        font.pixelSize: 9
                                                        elide: Text.ElideMiddle
                                                        Layout.fillWidth: true
                                                    }
                                                }

                                                Text {
                                                    text: modelData.extension
                                            Layout.minimumWidth: 65
                                            Layout.preferredWidth: 65
                                            Layout.maximumWidth: 65
                                                    color: "#59665D"
                                                    font.pixelSize: 11
                                                }

                                                Text {
                                                    text: modelData.sizeText
                                            Layout.minimumWidth: 75
                                            Layout.preferredWidth: 75
                                            Layout.maximumWidth: 75
                                                    color: "#59665D"
                                                    font.pixelSize: 11
                                                }

                                                Text {
                                                    text: modelData.lastModifiedText
                                                    Layout.fillWidth: true
                                                    color: "#59665D"
                                                    font.pixelSize: 10
                                                }

                                                Text {
                                                    text: modelData.status
                                            Layout.minimumWidth: 75
                                            Layout.preferredWidth: 75
                                            Layout.maximumWidth: 75
                                                    color: modelData.status === "inactive"
                                                           ? "#E5B66B" : "#426C52"
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                            }

                                            Rectangle {
                                                anchors.bottom: parent.bottom
                                                width: parent.width
                                                height: 1
                                                color: "#D7E0D6"
                                            }
                                        }
                                    }

                                    Text {
                                        visible: dashboard.scanFiles.length === 0
                                        text: "No file records available. Run a scan first."
                                        color: "#68756C"
                                        font.pixelSize: 12
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 55
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }

                            Item {
                                Layout.fillHeight: true
                            }
                        }
                    }
                }

                Item {
                    visible: window.currentPage === "Settings"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ScrollView {
                        anchors.fill: parent
                        clip: true
                        contentWidth: availableWidth

                        ColumnLayout {
                            width: parent.width
                            spacing: 18

                            Text {
                                text: "Settings"
                                color: "#252B29"
                                font.pixelSize: 21
                                font.bold: true
                            }

                            Text {
                                text: "Manage your SmartFileVault desktop preferences."
                                color: "#68756C"
                                font.pixelSize: 13
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 190
                                radius: 12
                                color: "#FFFCF7"
                                border.color: "#D7E0D6"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 20
                                    spacing: 12

                                    Text {
                                        text: "Default Inactivity Period"
                                        color: "#252B29"
                                        font.pixelSize: 16
                                        font.bold: true
                                    }

                                    Text {
                                        text: "Files inactive for this many days or longer can be considered for archiving during a scan."
                                        color: "#68756C"
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    RowLayout {
                                        spacing: 12

                                        SpinBox {
                                            id: settingsInactivity
                                            from: 1
                                            to: 365
                                            editable: true
                                            value: dashboard.defaultInactivityDays

            background: Rectangle {
                radius: 7
                color: parent.activeFocus ? "#F1F6EF"
                       : parent.hovered ? "#F7F9F5"
                       : "#FFFCF7"
                border.width: parent.activeFocus ? 2 : parent.hovered ? 1.5 : 1
                border.color: parent.activeFocus ? "#536F5B"
                             : parent.hovered ? "#8AA58D"
                             : "#CBD5CA"
            }
        }

                                        Text {
                                            text: "days"
                                            color: "#59665D"
                                            font.pixelSize: 12
                                        }

                                        Button {
                                            text: "Save Preference"
                                            onClicked: dashboard.setDefaultInactivityDays(settingsInactivity.value)

            background: Rectangle {
                radius: 8
                color: !parent.enabled ? "#E4E8E2"
                       : parent.down ? "#405B47"
                       : parent.hovered ? "#6F8D73"
                       : "#536F5B"
                border.color: parent.hovered ? "#405B47" : "#B8C7BB"
                opacity: parent.down ? 0.80 : parent.hovered ? 0.93 : 1.0
            }
        }
                                    }

                                    Text {
                                        text: dashboard.settingsStatus
                                        color: "#426C52"
                                        font.pixelSize: 11
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 145
                                radius: 12
                                color: "#FFFCF7"
                                border.color: "#D7E0D6"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 20
                                    spacing: 10

                                    Text {
                                        text: "Vault Information"
                                        color: "#252B29"
                                        font.pixelSize: 16
                                        font.bold: true
                                    }

                                    Text {
                                        text: "Local vault location"
                                        color: "#68756C"
                                        font.pixelSize: 11
                                    }

                                    Text {
                                        text: dashboard.vaultLocation
                                        color: "#59665D"
                                        font.pixelSize: 12
                                        wrapMode: Text.WrapAnywhere
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 105
                                radius: 12
                                color: "#FFFCF7"
                                border.color: "#D7E0D6"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 20
                                    spacing: 8

                                    Text {
                                        text: "Application Information"
                                        color: "#252B29"
                                        font.pixelSize: 16
                                        font.bold: true
                                    }

                                    Text {
                                        text: "SmartFileVault — Desktop Application"
                                        color: "#59665D"
                                        font.pixelSize: 12
                                    }
                                }
                            }

                            Item {
                                Layout.fillHeight: true
                            }
                        }
                    }
                }

                Item {
                    visible: window.currentPage !== "Dashboard"
                             && window.currentPage !== "Archives"
                             && window.currentPage !== "Scan"
                             && window.currentPage !== "Restore"
                             && window.currentPage !== "Files"
                             && window.currentPage !== "Settings"
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 12

                        Text {
                            text: window.currentPage
                            color: "#252B29"
                            font.pixelSize: 23
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }

                        Text {
                            text: "This section will be connected to the existing backend."
                            color: "#68756C"
                            font.pixelSize: 13
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }

                Item {
                    visible: window.currentPage === "Dashboard"
                    Layout.fillHeight: true
                }
            }
        }
    }
}
