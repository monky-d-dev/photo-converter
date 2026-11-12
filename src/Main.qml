import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: win
    objectName: "mainWindow"
    width: 1320
    height: 880
    minimumWidth: 1080
    minimumHeight: 740
    visible: true
    title: qsTr("Photo Converter · Graduaciones")
    color: colors.canvas
    font.family: Qt.platform.os === "windows" ? "Segoe UI" : "Noto Sans"
    font.pixelSize: 13
    palette.window: colors.surface
    palette.windowText: colors.text
    palette.base: colors.field
    palette.alternateBase: colors.panel
    palette.text: colors.text
    palette.button: colors.panel
    palette.buttonText: colors.text
    palette.highlight: colors.accent
    palette.highlightedText: colors.accentText
    palette.placeholderText: colors.secondaryText
    palette.toolTipBase: colors.surface
    palette.toolTipText: colors.text
    palette.light: colors.field
    palette.midlight: colors.divider
    palette.mid: colors.border
    palette.dark: colors.text
    palette.shadow: colors.scrim
    palette.link: colors.accent
    palette.linkVisited: colors.accent
    palette.brightText: colors.text
    palette.disabled.windowText: colors.disabledText
    palette.disabled.text: colors.disabledText
    palette.disabled.buttonText: colors.disabledText
    palette.disabled.placeholderText: colors.disabledText
    palette.disabled.base: colors.disabledSurface
    palette.disabled.button: colors.disabledSurface
    palette.disabled.highlight: colors.disabledBorder
    palette.disabled.highlightedText: colors.disabledText
    palette.inactive.windowText: colors.text
    palette.inactive.text: colors.text
    palette.inactive.buttonText: colors.text
    palette.inactive.base: colors.field
    palette.inactive.highlight: colors.accent
    palette.inactive.highlightedText: colors.accentText

    ThemePalette { id: colors; objectName: "themePalette"; dark: appearance.dark }
    readonly property color ink: colors.text
    readonly property color muted: colors.secondaryText
    readonly property color accent: colors.accent
    property int cropIndex: 0
    property bool showGuides: true
    property bool allowClose: false
    readonly property bool actionShortcutsEnabled: !backend.exporting && !exportResultDialog.visible
    property var crop: backend.currentCrops[cropIndex]
    readonly property var slotRects: [
        {x:0,y:0,w:3,h:2,c:0,label:"3 × 2"}, {x:3,y:0,w:3,h:2,c:0,label:"3 × 2"},
        {x:0,y:2,w:3,h:2,c:0,label:"3 × 2"}, {x:3,y:2,w:3,h:2,c:0,label:"3 × 2"},
        {x:0,y:4,w:4,h:3,c:1,label:"4 × 3"}, {x:0,y:7,w:4,h:3,c:1,label:"4 × 3"},
        {x:4,y:4,w:4,h:6,c:2,label:"4 × 6"}
    ]

    function setCrop(x, y, zoom) {
        backend.setCrop(cropIndex, Math.max(0, Math.min(1, x)), Math.max(0, Math.min(1, y)), Math.max(1, Math.min(3, zoom)))
    }
    function exportBatch() {
        if (backend.count && !backend.busy && !exportResultDialog.visible) {
            if (!backend.destination.length) targetFolder.open()
            else backend.exportAll(formatBox.currentText.toLowerCase(), pendingOnly.checked)
        }
    }
    onClosing: function(close) {
        if (!allowClose && backend.exporting) { close.accepted = false; return }
        if (!allowClose && (backend.dirty || backend.busy)) { close.accepted = false; closeDialog.open() }
    }

    component ActionButton: Button {
        id: control
        property bool primary: false
        property bool quiet: false
        padding: 12
        topPadding: 9
        bottomPadding: 9
        implicitHeight: 40
        hoverEnabled: true
        font.pixelSize: 13
        font.weight: primary ? Font.DemiBold : Font.Medium
        contentItem: Text {
            text: control.text; font: control.font
            color: !control.enabled ? colors.disabledText : control.primary ? colors.accentText : colors.text
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 5
            color: !control.enabled ? colors.disabledSurface : control.primary ? (control.hovered ? colors.accentHover : colors.accent)
                : control.hovered ? colors.hover : control.quiet ? "transparent" : colors.panel
            border.color: control.primary || control.quiet ? "transparent" : control.enabled ? colors.border : colors.disabledBorder
            Rectangle {
                anchors.fill: parent; anchors.margins: -3
                radius: 7; color: "transparent"; border.width: 2; border.color: colors.focus
                visible: control.activeFocus
            }
        }
    }
    component AppCheckBox: CheckBox {
        id: control
        implicitHeight: 36; spacing: 9; padding: 4
        hoverEnabled: true; font.pixelSize: 13
        indicator: Rectangle {
            x: control.leftPadding; y: (control.height - height) / 2
            implicitWidth: 22; implicitHeight: 22; radius: 3
            color: !control.enabled ? colors.disabledSurface : control.checked ? colors.accent : control.hovered ? colors.hover : colors.field
            border.width: control.activeFocus ? 3 : 2
            border.color: !control.enabled ? colors.disabledBorder : control.activeFocus ? colors.focus : control.checked ? colors.accent : colors.border
            Text {
                anchors.centerIn: parent; text: control.checkState === Qt.PartiallyChecked ? "−" : "✓"
                visible: control.checkState !== Qt.Unchecked
                font.pixelSize: 17; font.bold: true
                color: control.enabled ? colors.accentText : colors.disabledText
            }
        }
        contentItem: Text {
            text: control.text; font: control.font
            color: control.enabled ? colors.text : colors.disabledText
            leftPadding: control.indicator.width + control.spacing
            verticalAlignment: Text.AlignVCenter
        }
    }
    component AppScrollBar: ScrollBar {
        id: control
        visible: policy === ScrollBar.AlwaysOn || (policy === ScrollBar.AsNeeded && size < 1)
        x: orientation === Qt.Vertical && parent ? parent.width - width : 0
        y: orientation === Qt.Horizontal && parent ? parent.height - height : 0
        contentItem: Rectangle {
            implicitWidth: 8; implicitHeight: 24; radius: 4
            color: control.pressed || control.hovered ? colors.accent : colors.border
        }
        background: Rectangle { color: colors.canvas; radius: 4 }
    }
    component AppComboBox: ComboBox {
        id: control
        implicitHeight: 38
        leftPadding: 12; rightPadding: 30
        hoverEnabled: true; font.pixelSize: 13
        contentItem: Text {
            text: control.displayText; font: control.font
            color: control.enabled ? colors.text : colors.disabledText
            verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
        }
        indicator: Canvas {
            x: control.width - width - 12; y: (control.height - height) / 2
            width: 12; height: 7
            property color lineColor: control.enabled ? colors.text : colors.disabledText
            onLineColorChanged: requestPaint()
            onPaint: {
                const context = getContext("2d"); context.clearRect(0, 0, width, height)
                context.strokeStyle = lineColor; context.lineWidth = 2
                context.beginPath(); context.moveTo(1, 1); context.lineTo(6, 6); context.lineTo(11, 1); context.stroke()
            }
        }
        background: Rectangle {
            color: !control.enabled ? colors.disabledSurface : control.hovered ? colors.hover : colors.field
            radius: 5; border.width: control.activeFocus ? 2 : 1
            border.color: control.activeFocus ? colors.focus : control.enabled ? colors.border : colors.disabledBorder
        }
        delegate: ItemDelegate {
            id: option
            required property var modelData
            required property int index
            width: control.width - 8; implicitHeight: 38
            highlighted: control.highlightedIndex === index
            contentItem: Text {
                text: option.modelData; font: control.font; color: colors.text
                verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
            }
            background: Rectangle {
                radius: 3; color: option.highlighted ? colors.selected : option.hovered ? colors.hover : colors.surface
                border.width: option.highlighted ? 1 : 0; border.color: colors.focus
            }
        }
        popup: Popup {
            y: control.height + 4; width: control.width; padding: 4
            implicitHeight: Math.min(contentItem.implicitHeight + 8, 280)
            contentItem: ListView {
                clip: true; implicitHeight: contentHeight
                model: control.popup.visible ? control.delegateModel : null
                currentIndex: control.highlightedIndex
                ScrollBar.vertical: AppScrollBar {}
            }
            background: Rectangle { color: colors.surface; border.color: colors.border; radius: 5 }
        }
    }
    component AppSpinBox: SpinBox {
        id: control
        padding: 0; leftPadding: 33; rightPadding: 33
        implicitHeight: 36; implicitWidth: 120; font.pixelSize: 13
        contentItem: TextInput {
            id: editor
            objectName: "marginEditor"
            z: 2; text: control.textFromValue(control.value, control.locale)
            font: control.font; color: control.enabled ? colors.text : colors.disabledText
            selectionColor: colors.accent; selectedTextColor: colors.accentText
            horizontalAlignment: TextInput.AlignHCenter; verticalAlignment: TextInput.AlignVCenter
            readOnly: !control.editable; validator: control.validator
            inputMethodHints: Qt.ImhFormattedNumbersOnly; clip: true
        }
        up.indicator: Rectangle {
            objectName: "marginIncrease"
            x: control.width - width; height: control.height; implicitWidth: 32
            color: !control.enabled ? colors.disabledSurface : control.up.pressed ? colors.selected : control.up.hovered ? colors.hover : colors.panel
            border.color: control.enabled ? colors.border : colors.disabledBorder; radius: 4
            Text {
                anchors.centerIn: parent; text: "+"; font.pixelSize: 21
                color: control.enabled && control.value < control.to ? colors.text : colors.disabledText
            }
        }
        down.indicator: Rectangle {
            objectName: "marginDecrease"
            x: 0; height: control.height; implicitWidth: 32
            color: !control.enabled ? colors.disabledSurface : control.down.pressed ? colors.selected : control.down.hovered ? colors.hover : colors.panel
            border.color: control.enabled ? colors.border : colors.disabledBorder; radius: 4
            Text {
                anchors.centerIn: parent; text: "−"; font.pixelSize: 21
                color: control.enabled && control.value > control.from ? colors.text : colors.disabledText
            }
        }
        background: Rectangle {
            color: control.enabled ? colors.field : colors.disabledSurface
            radius: 4; border.width: control.activeFocus || editor.activeFocus ? 2 : 1
            border.color: control.activeFocus || editor.activeFocus ? colors.focus : control.enabled ? colors.border : colors.disabledBorder
        }
    }
    component AppSlider: Slider {
        id: control
        implicitHeight: 34; padding: 6; hoverEnabled: true
        background: Rectangle {
            x: control.leftPadding; y: control.topPadding + (control.availableHeight - height) / 2
            width: control.availableWidth; height: 5; radius: 2
            color: control.enabled ? colors.track : colors.disabledBorder
            Rectangle { width: control.visualPosition * parent.width; height: parent.height; radius: 2; color: control.enabled ? colors.accent : colors.disabledText }
        }
        handle: Rectangle {
            x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
            y: control.topPadding + (control.availableHeight - height) / 2
            implicitWidth: 23; implicitHeight: 23; radius: width / 2
            color: control.enabled ? colors.field : colors.disabledSurface
            border.width: control.activeFocus ? 3 : 2
            border.color: !control.enabled ? colors.disabledBorder : control.activeFocus ? colors.focus : colors.accent
        }
    }
    component AppProgressBar: ProgressBar {
        id: control
        background: Rectangle { implicitHeight: 5; color: colors.track; radius: 2 }
        contentItem: Item {
            implicitHeight: 5; clip: true
            Rectangle {
                property real phase: 0
                width: parent.width * (control.indeterminate ? .25 : control.position)
                height: parent.height; radius: 2; color: colors.accent
                x: control.indeterminate ? phase * (parent.width + width) - width : 0
                NumberAnimation on phase { running: control.indeterminate && control.visible; from: 0; to: 1; duration: 1300; loops: Animation.Infinite }
            }
        }
    }
    component AppDialog: Dialog {
        id: control
        padding: 20
        header: Label {
            text: control.title; visible: text.length > 0; color: colors.text
            font.pixelSize: 17; font.weight: Font.DemiBold
            padding: 20; bottomPadding: 8; wrapMode: Text.WordWrap
        }
        background: Rectangle { color: colors.surface; border.color: colors.border; radius: 7 }
        Overlay.modal: Rectangle { color: colors.scrim }
    }
    component AppDialogButtons: DialogButtonBox {
        padding: 14; spacing: 9
        background: Rectangle { color: colors.surface; radius: 7 }
    }
    component AppTextArea: TextArea {
        color: colors.text; placeholderTextColor: colors.secondaryText
        selectionColor: colors.accent; selectedTextColor: colors.accentText
        padding: 12; font.pixelSize: 13
        background: Rectangle { color: colors.field; border.color: colors.border; radius: 4 }
    }
    component SmallLabel: Label { color: colors.secondaryText; font.pixelSize: 12; wrapMode: Text.WordWrap }

    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; enabled: !backend.busy && win.actionShortcutsEnabled; onActivated: imagesDialog.open() }
    Shortcut { sequence: "Ctrl+Shift+O"; context: Qt.ApplicationShortcut; enabled: !backend.busy && win.actionShortcutsEnabled; onActivated: sourceFolder.open() }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: backend.canUndo && win.actionShortcutsEnabled; onActivated: backend.undo() }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; context: Qt.ApplicationShortcut; enabled: backend.canRedo && win.actionShortcutsEnabled; onActivated: backend.redo() }
    Shortcut { sequence: "Ctrl+Return"; context: Qt.ApplicationShortcut; enabled: win.actionShortcutsEnabled; onActivated: win.exportBatch() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: win.actionShortcutsEnabled; onActivated: win.exportBatch() }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: win.actionShortcutsEnabled; onActivated: helpDialog.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: win.actionShortcutsEnabled; onActivated: win.close() }

    FileDialog {
        id: imagesDialog
        objectName: "imagesDialog"
        title: qsTr("Seleccionar fotografías")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Fotografías (*.jpg *.jpeg *.png)"), qsTr("Todos los archivos (*)")]
        onAccepted: backend.addUrls(selectedFiles, recursiveCheck.checked)
    }
    FolderDialog {
        id: sourceFolder
        objectName: "sourceFolderDialog"
        title: qsTr("Seleccionar carpeta de fotografías")
        onAccepted: backend.addUrls([selectedFolder], recursiveCheck.checked)
    }
    FolderDialog {
        id: targetFolder
        objectName: "targetFolderDialog"
        currentFolder: backend.destinationUrl
        title: qsTr("Seleccionar carpeta de salida, fuera del origen")
        onAccepted: backend.setTargetFolder(selectedFolder)
    }

    Item {
        id: workspace
        objectName: "workspace"
        anchors.fill: parent
        enabled: !backend.exporting
    Rectangle {
        id: toolbar
        height: 78
        anchors { top: parent.top; left: parent.left; right: parent.right }
        color: colors.surface
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: colors.divider }
        RowLayout {
            anchors { fill: parent; leftMargin: 24; rightMargin: 24 }
            spacing: 12
            Rectangle {
                Layout.preferredWidth: 38; Layout.preferredHeight: 46; color: win.accent; radius: 4
                Grid {
                    anchors.centerIn: parent; columns: 2; spacing: 3
                    Repeater { model: 4; Rectangle { width: 9; height: 11; color: colors.accentText; opacity: .95 } }
                }
            }
            Column {
                spacing: 1
                Label { text: "Photo Converter"; color: win.ink; font.pixelSize: 21; font.weight: Font.DemiBold }
                SmallLabel { text: qsTr("Paquetes de graduación") }
            }
            Item { Layout.fillWidth: true }
            ActionButton { objectName: "addImages"; text: qsTr("Agregar fotos"); enabled: !backend.busy; onClicked: imagesDialog.open() }
            ActionButton { text: qsTr("Cargar carpeta"); enabled: !backend.busy; onClicked: sourceFolder.open() }
            Label { text: qsTr("Tema"); color: colors.secondaryText; font.pixelSize: 12 }
            AppComboBox {
                objectName: "themePicker"
                Layout.preferredWidth: 116
                model: [qsTr("Claro"), qsTr("Oscuro")]
                currentIndex: appearance.dark ? 1 : 0
                onActivated: function(index) { appearance.theme = index === 1 ? "dark" : "light" }
                Accessible.name: qsTr("Tema de la aplicación")
            }
            ActionButton { text: "?"; implicitWidth: 38; quiet: true; Accessible.name: qsTr("Ayuda y atajos"); onClicked: helpDialog.open() }
        }
    }

    RowLayout {
        anchors { top: toolbar.bottom; left: parent.left; right: parent.right; bottom: exportBar.top }
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 248; Layout.fillHeight: true; color: colors.panel
            Rectangle { anchors.right: parent.right; height: parent.height; width: 1; color: colors.divider }
            ColumnLayout {
                anchors.fill: parent; spacing: 0
                ColumnLayout {
                    Layout.fillWidth: true; Layout.margins: 18; spacing: 5
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Fotografías"); color: win.ink; font.pixelSize: 16; font.weight: Font.DemiBold; Layout.fillWidth: true }
                        Label { text: backend.count; color: win.accent; font.pixelSize: 18; font.weight: Font.DemiBold }
                    }
                    AppCheckBox {
                        id: recursiveCheck
                        objectName: "recursiveCheck"
                        text: qsTr("Incluir subcarpetas"); checked: true; enabled: !backend.busy
                        font.pixelSize: 12
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Se aplica a la próxima carpeta que cargues.")
                    }
                }
                ListView {
                    id: photoList
                    objectName: "photoList"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    clip: true
                    model: backend.photos
                    spacing: 2
                    ScrollBar.vertical: AppScrollBar {}
                    delegate: ItemDelegate {
                        id: photoRow
                        required property var photoData
                        required property int index
                        objectName: "photoRow_" + index
                        width: photoList.width
                        height: 86
                        leftPadding: 16; rightPadding: 14
                        onClicked: backend.select(index)
                        background: Rectangle {
                            color: photoRow.index === backend.selected ? colors.selected : photoRow.hovered ? colors.canvas : "transparent"
                            Rectangle { width: 3; height: parent.height; color: win.accent; visible: photoRow.index === backend.selected }
                        }
                        contentItem: RowLayout {
                            spacing: 11
                            Rectangle {
                                Layout.preferredWidth: 49; Layout.preferredHeight: 63; color: colors.disabledSurface; clip: true
                                Image { anchors.fill: parent; source: photoRow.photoData.url; sourceSize: Qt.size(98,126); autoTransform: true; asynchronous: true; fillMode: Image.PreserveAspectCrop }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Label { text: photoRow.photoData.name; color: win.ink; font.weight: Font.DemiBold; elide: Text.ElideMiddle; Layout.fillWidth: true }
                                Label { text: photoRow.photoData.dimensions; color: win.muted; font.pixelSize: 12 }
                                Label {
                                    text: photoRow.photoData.status === "Error" ? qsTr("Error · ver detalles")
                                        : photoRow.photoData.warning ? photoRow.photoData.status + qsTr(" · revisar ppp") : photoRow.photoData.status
                                    color: photoRow.photoData.status === "Error" ? colors.error : photoRow.photoData.status === "Exportada" ? colors.success : photoRow.photoData.warning ? colors.warningText : win.muted
                                    font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true
                                }
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: photoData.relative
                    }
                    Label {
                        anchors { centerIn: parent; margins: 24 }
                        width: parent.width - 48
                        visible: backend.count === 0
                        text: qsTr("Agrega las fotos de un alumno o de todo el grupo.\n\nTambién puedes arrastrar imágenes y carpetas a esta ventana.")
                        color: win.muted; wrapMode: Text.WordWrap; lineHeight: 1.3; horizontalAlignment: Text.AlignHCenter
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; Layout.margins: 12; spacing: 4
                    ActionButton { text: qsTr("Quitar"); quiet: true; Layout.fillWidth: true; enabled: backend.count > 0 && !backend.busy; onClicked: removeDialog.open() }
                    ActionButton { text: qsTr("Vaciar lote"); quiet: true; Layout.fillWidth: true; enabled: backend.count > 0 && !backend.busy; onClicked: clearDialog.open() }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            spacing: 0
            RowLayout {
                Layout.fillWidth: true; Layout.leftMargin: 22; Layout.rightMargin: 22; Layout.topMargin: 17; Layout.bottomMargin: 10
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 3
                    Label { text: qsTr("Hoja de impresión"); color: win.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
                    SmallLabel { text: backend.count ? backend.currentName : qsTr("Una composición por fotografía"); elide: Text.ElideMiddle; wrapMode: Text.NoWrap; Layout.fillWidth: true }
                }
                AppCheckBox { objectName: "guidesCheck"; text: qsTr("Guías"); checked: win.showGuides; onToggled: win.showGuides = checked; font.pixelSize: 12 }
            }
            Rectangle {
                objectName: "importErrorBanner"
                Layout.fillWidth: true; Layout.leftMargin: 22; Layout.rightMargin: 22; Layout.bottomMargin: 8
                visible: backend.importError.length > 0
                implicitHeight: importErrorText.implicitHeight + 20
                color: colors.errorSurface; radius: 5; border.color: colors.error
                Label {
                    id: importErrorText
                    anchors { fill: parent; margins: 10 }
                    text: backend.importError; textFormat: Text.PlainText
                    color: colors.error; wrapMode: Text.WordWrap; font.pixelSize: 13
                }
            }
            Item {
                id: paperArea
                Layout.fillWidth: true; Layout.fillHeight: true
                Rectangle {
                    id: paper
                    objectName: "paper"
                    anchors.centerIn: parent
                    width: Math.floor(Math.min(paperArea.width - 52, (paperArea.height - 24) * .8))
                    height: width * 1.25
                    color: colors.paper
                    Image {
                        id: sheetImage
                        objectName: "sheetImage"
                        anchors.fill: parent
                        source: backend.count ? "image://preview/sheet/" + backend.revision : ""
                        cache: false
                        asynchronous: false
                        fillMode: Image.Stretch
                    }
                    Repeater {
                        model: win.slotRects
                        Rectangle {
                            required property var modelData
                            x: modelData.x / 8 * paper.width; y: modelData.y / 10 * paper.height
                            width: modelData.w / 8 * paper.width; height: modelData.h / 10 * paper.height
                            color: backend.count ? "transparent" : colors.paperPlaceholder
                            border.width: win.showGuides ? (modelData.c === win.cropIndex && backend.count ? 2 : 1) : 0
                            border.color: modelData.c === win.cropIndex && backend.count ? colors.paperAccent : colors.paperBorder
                            Label {
                                anchors.centerIn: parent
                                text: modelData.label + qsTr(" pulgadas")
                                visible: !backend.count
                                color: colors.paperMuted; font.pixelSize: 13
                            }
                            Rectangle {
                                anchors { left: parent.left; top: parent.top; margins: 5 }
                                width: sizeTag.implicitWidth + 10; height: 22; radius: 2; color: colors.paperTag
                                visible: win.showGuides && backend.count > 0
                                Label { id: sizeTag; anchors.centerIn: parent; text: modelData.label; font.pixelSize: 12; color: colors.paperText }
                            }
                            MouseArea { anchors.fill: parent; onClicked: win.cropIndex = modelData.c; cursorShape: Qt.PointingHandCursor }
                        }
                    }
                    Label {
                        x: paper.width * .75; y: paper.height * .17; width: paper.width * .25
                        text: qsTr("Espacio\nlibre"); horizontalAlignment: Text.AlignHCenter
                        color: colors.paperMuted; font.pixelSize: 12; visible: win.showGuides
                    }
                    Label {
                        anchors.centerIn: parent; visible: backend.count && sheetImage.status === Image.Error
                        width: parent.width - 40; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
                        text: qsTr("No se pudo leer esta fotografía. Comprueba el archivo original."); color: "#a12b36"
                    }
                }
            }
            SmallLabel {
                Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 8; Layout.bottomMargin: 17
                text: qsTr("8 × 10 pulgadas   ·   2400 × 3000 px   ·   300 ppp")
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Rectangle {
            Layout.preferredWidth: 288; Layout.fillHeight: true; color: colors.surface
            Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: colors.divider }
            ScrollView {
                anchors.fill: parent
                contentWidth: availableWidth
                clip: true
                ScrollBar.vertical: AppScrollBar {}
                ColumnLayout {
                    width: parent.width
                    spacing: 8
                    Item { Layout.preferredHeight: 4 }
                    Label { text: qsTr("Encuadre por tamaño"); color: win.ink; font.pixelSize: 16; font.weight: Font.DemiBold; Layout.leftMargin: 18 }
                    SmallLabel { text: qsTr("Solo esta foto; las copias del mismo tamaño se ajustan juntas."); Layout.fillWidth: true; Layout.leftMargin: 18; Layout.rightMargin: 18 }
                    RowLayout {
                        Layout.fillWidth: true; Layout.leftMargin: 18; Layout.rightMargin: 18; spacing: 5
                        Repeater {
                            model: ["3 × 2", "4 × 3", "4 × 6"]
                            ActionButton {
                                required property string modelData
                                required property int index
                                objectName: "ratio_" + index
                                text: modelData; primary: win.cropIndex === index; Layout.fillWidth: true
                                onClicked: win.cropIndex = index
                            }
                        }
                    }
                    Rectangle {
                        id: cropArea
                        objectName: "cropArea"
                        Layout.fillWidth: true; Layout.leftMargin: 18; Layout.rightMargin: 18; Layout.preferredHeight: 172
                        color: colors.canvas; clip: true
                        Image {
                            id: cropImage
                            anchors { fill: parent; margins: 8 }
                            source: backend.count ? "image://preview/crop/" + win.cropIndex + "/" + backend.revision : ""
                            cache: false; asynchronous: false; fillMode: Image.PreserveAspectFit
                        }
                        SmallLabel { anchors.centerIn: parent; text: qsTr("Carga una fotografía"); visible: !backend.count }
                        MouseArea {
                            anchors.fill: parent
                            enabled: backend.count > 0 && !backend.busy
                            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                            preventStealing: true
                            property real previousX: 0
                            property real previousY: 0
                            onPressed: function(mouse) { previousX = mouse.x; previousY = mouse.y }
                            onPositionChanged: function(mouse) {
                                if (pressed) {
                                    win.setCrop(win.crop.x - (mouse.x - previousX) / width,
                                                win.crop.y - (mouse.y - previousY) / height, win.crop.zoom)
                                    previousX = mouse.x; previousY = mouse.y
                                }
                            }
                            onWheel: function(wheel) { win.setCrop(win.crop.x, win.crop.y, win.crop.zoom + wheel.angleDelta.y / 1200) }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.leftMargin: 18; Layout.rightMargin: 18; spacing: 2
                        enabled: backend.count > 0 && !backend.busy
                        RowLayout {
                            Layout.fillWidth: true; Layout.bottomMargin: 8; spacing: 10
                            Label { text: qsTr("Giro"); color: win.ink }
                            AppComboBox {
                                objectName: "rotationBox"
                                Layout.fillWidth: true; implicitHeight: 36; font.pixelSize: 12
                                model: [qsTr("Sin giro"), qsTr("90° a la derecha"), qsTr("180°"), qsTr("90° a la izquierda")]
                                currentIndex: win.crop.rotation / 90
                                onActivated: function(index) { backend.setRotation(win.cropIndex, index * 90) }
                                Accessible.name: qsTr("Giro de la foto para este tamaño")
                            }
                        }
                        RowLayout {
                            Label { text: qsTr("Ampliación"); color: win.ink; Layout.fillWidth: true }
                            SmallLabel { text: Math.round(win.crop.zoom * 100) + " %" }
                        }
                        AppSlider { objectName: "zoomSlider"; Layout.fillWidth: true; from: 1; to: 3; value: win.crop.zoom; onMoved: win.setCrop(win.crop.x, win.crop.y, value) }
                        Label { text: qsTr("Posición horizontal"); color: win.ink }
                        AppSlider { objectName: "xSlider"; Layout.fillWidth: true; from: 0; to: 1; value: win.crop.x; onMoved: win.setCrop(value, win.crop.y, win.crop.zoom) }
                        Label { text: qsTr("Posición vertical"); color: win.ink }
                        AppSlider { objectName: "ySlider"; Layout.fillWidth: true; from: 0; to: 1; value: win.crop.y; onMoved: win.setCrop(win.crop.x, value, win.crop.zoom) }
                    }
                    RowLayout {
                        Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; spacing: 0
                        ActionButton { text: qsTr("Restablecer"); quiet: true; Layout.fillWidth: true; enabled: backend.count > 0 && !backend.busy; onClicked: backend.resetCrops() }
                        ActionButton { text: qsTr("Deshacer"); quiet: true; Layout.fillWidth: true; enabled: backend.canUndo; onClicked: backend.undo() }
                    }
                    Rectangle { Layout.fillWidth: true; Layout.leftMargin: 18; Layout.rightMargin: 18; height: 1; color: colors.divider }
                    ActionButton {
                        objectName: "applyToAll"
                        text: qsTr("Usar encuadres en todo el lote")
                        Layout.fillWidth: true; Layout.leftMargin: 18; Layout.rightMargin: 18
                        enabled: backend.count > 0 && !backend.busy
                        onClicked: applyDialog.open()
                    }
                    SmallLabel { text: qsTr("Después puedes corregir cada alumno por separado."); Layout.fillWidth: true; Layout.leftMargin: 18; Layout.rightMargin: 18 }
                    Rectangle {
                        Layout.fillWidth: true; Layout.leftMargin: 18; Layout.rightMargin: 18
                        implicitHeight: warningText.implicitHeight + 20
                        color: backend.currentError.length ? colors.errorSurface : colors.warningSurface; radius: 4
                        visible: backend.currentWarning.length > 0 || backend.currentError.length > 0
                        Label {
                            id: warningText
                            anchors { fill: parent; margins: 10 }
                            text: backend.currentError.length ? backend.currentError : backend.currentWarning
                            color: backend.currentError.length ? colors.error : colors.warningText; wrapMode: Text.WordWrap; font.pixelSize: 12
                        }
                    }
                    Item { Layout.preferredHeight: 8 }
                }
            }
        }
    }

    Rectangle {
        id: exportBar
        height: 160
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        color: colors.surface
        Rectangle { height: 1; width: parent.width; color: colors.divider }
        ColumnLayout {
            anchors { fill: parent; leftMargin: 22; rightMargin: 22; topMargin: 13; bottomMargin: 10 }
            spacing: 8
            RowLayout {
                Layout.fillWidth: true; spacing: 10
                ActionButton { text: qsTr("Carpeta de salida"); enabled: !backend.busy; onClicked: targetFolder.open() }
                SmallLabel {
                    Layout.fillWidth: true
                    text: backend.destination.length ? backend.destination : qsTr("Elige dónde guardar las composiciones")
                    elide: Text.ElideMiddle; wrapMode: Text.NoWrap
                    ToolTip.visible: destinationHover.hovered
                    ToolTip.text: backend.destination
                    HoverHandler { id: destinationHover }
                }
                AppComboBox { id: formatBox; objectName: "formatBox"; model: ["JPG", "PNG", "PDF"]; implicitWidth: 88; enabled: !backend.busy }
                AppCheckBox {
                    objectName: "attach8x10Check"
                    text: qsTr("Adjuntar 8x10")
                    checked: backend.attach8x10
                    enabled: !backend.busy
                    font.pixelSize: 12
                    onToggled: backend.setAttach8x10(checked)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Añade una foto de 8 de ancho × 10 de alto a 300 ppp, sin márgenes. Usa el giro, posición y ampliación de 4 × 6, adaptados a 8 × 10.")
                }
                AppCheckBox { id: pendingOnly; objectName: "pendingOnly"; text: qsTr("Solo pendientes"); checked: true; enabled: !backend.busy; font.pixelSize: 12 }
                ActionButton {
                    objectName: "exportButton"
                    text: backend.busy ? qsTr("Cancelar") : qsTr("Exportar lote")
                    primary: true; implicitWidth: 150
                    enabled: backend.busy || backend.count > 0
                    onClicked: backend.busy ? backend.cancel() : win.exportBatch()
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 10
                Label { text: qsTr("Margen interior (mm)"); color: win.ink; font.pixelSize: 12 }
                AppSpinBox {
                    id: marginBox
                    objectName: "marginBox"
                    from: 0; to: 100; stepSize: 1
                    value: Math.round(backend.marginMm * 10)
                    implicitWidth: 114; implicitHeight: 32
                    editable: true; enabled: !backend.busy
                    textFromValue: function(value, locale) { return Number(value / 10).toLocaleString(locale, 'f', 1) }
                    valueFromText: function(text, locale) { return Math.round(Number.fromLocaleString(locale, text) * 10) }
                    validator: DoubleValidator { bottom: 0; top: 10; decimals: 1; locale: marginBox.locale.name }
                    onValueModified: backend.setMarginMm(value / 10)
                    Accessible.name: qsTr("Margen de corte por lado de las líneas interiores, sin margen exterior, para todo el lote")
                }
                SmallLabel {
                    text: qsTr("En todo el lote. Blanco entre fotos: %1 mm.").arg(Number(backend.marginMm * 2).toLocaleString(Qt.locale(), 'f', 1))
                }
                SmallLabel {
                    text: backend.message; Layout.fillWidth: true; elide: Text.ElideMiddle; wrapMode: Text.NoWrap
                    ToolTip.visible: statusHover.hovered
                    ToolTip.text: backend.message
                    HoverHandler { id: statusHover }
                }
                ActionButton { text: qsTr("Ver detalles"); quiet: true; visible: backend.details.length > 0; implicitHeight: 28; onClicked: detailsDialog.open() }
                ActionButton { text: qsTr("Abrir salida"); quiet: true; visible: backend.destination.length > 0; implicitHeight: 28; onClicked: backend.openDestination() }
            }
            AppProgressBar { Layout.fillWidth: true; value: backend.progress; indeterminate: backend.scanning; visible: backend.busy || backend.progress > 0; Layout.preferredHeight: 5 }
            SmallLabel { text: qsTr("Los originales se conservan. Las guías no se exportan. Imprime al 100 % / tamaño real."); font.pixelSize: 12 }
        }
    }

    DropArea {
        anchors { top: toolbar.bottom; left: parent.left; right: parent.right; bottom: exportBar.top }
        enabled: !backend.busy
        onDropped: function(drop) { if (drop.hasUrls) backend.addUrls(drop.urls, recursiveCheck.checked) }
        Rectangle {
            anchors.fill: parent; visible: parent.containsDrag; color: colors.selected; opacity: .94; border.color: win.accent; border.width: 3
            Label { anchors.centerIn: parent; text: qsTr("Suelta aquí tus fotografías o carpetas"); color: win.accent; font.pixelSize: 24 }
        }
    }

    }

    Connections {
        target: backend
        function onExportFinished() { exportResultDialog.open() }
    }

    AppDialog {
        id: applyDialog
        anchors.centerIn: Overlay.overlay
        modal: true; width: 440
        title: qsTr("Aplicar encuadres al lote")
        Label { color: colors.text; width: parent.width; wrapMode: Text.WordWrap; text: qsTr("Se reemplazarán los tres encuadres, incluidos sus giros, de todas las fotografías por los de la foto seleccionada. Puedes deshacer este cambio con Ctrl+Z.") }
        footer: AppDialogButtons {
            ActionButton { text: qsTr("Cancelar"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            ActionButton { objectName: "confirmApply"; text: qsTr("Aplicar a todas"); primary: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: backend.applyToAll()
    }
    AppDialog {
        id: removeDialog
        anchors.centerIn: Overlay.overlay; modal: true; width: 410
        title: qsTr("Quitar fotografía del lote")
        Label { color: colors.text; width: parent.width; wrapMode: Text.WordWrap; text: qsTr("Se quitará la fotografía seleccionada y sus ajustes de esta sesión. El original y sus exportaciones se conservarán.") }
        footer: AppDialogButtons {
            ActionButton { text: qsTr("Conservar foto"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            ActionButton { text: qsTr("Quitar foto"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: backend.removeSelected()
    }
    AppDialog {
        id: clearDialog
        anchors.centerIn: Overlay.overlay; modal: true; width: 400
        title: qsTr("Vaciar el lote")
        Label { color: colors.text; width: parent.width; wrapMode: Text.WordWrap; text: qsTr("Se quitarán todas las fotografías y sus ajustes de esta sesión. Los originales y los archivos exportados se conservarán.") }
        footer: AppDialogButtons {
            ActionButton { text: qsTr("Conservar lote"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            ActionButton { text: qsTr("Vaciar lote"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: backend.clear()
    }
    AppDialog {
        id: closeDialog
        anchors.centerIn: Overlay.overlay; modal: true; width: 430
        title: qsTr("Cerrar Photo Converter")
        Label { color: colors.text; width: parent.width; wrapMode: Text.WordWrap; text: qsTr("Hay fotografías sin exportar o un lote en proceso. Al cerrar se perderán los ajustes de esta sesión. Los archivos terminados se conservarán.") }
        footer: AppDialogButtons {
            ActionButton { text: qsTr("Seguir trabajando"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            ActionButton { text: qsTr("Cerrar"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: { backend.cancel(); win.allowClose = true; win.close() }
    }
    AppDialog {
        id: detailsDialog
        objectName: "detailsDialog"
        anchors.centerIn: Overlay.overlay; modal: true; width: Math.min(680, win.width - 80); height: 430
        title: qsTr("Archivos que necesitan revisión")
        ScrollView {
            anchors.fill: parent; contentWidth: availableWidth
            ScrollBar.vertical: AppScrollBar {}
            AppTextArea { text: backend.details; readOnly: true; wrapMode: TextEdit.Wrap; selectByMouse: true }
        }
        footer: AppDialogButtons { ActionButton { text: qsTr("Cerrar"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole } }
    }
    AppDialog {
        id: helpDialog
        objectName: "helpDialog"
        anchors.centerIn: Overlay.overlay; modal: true; width: 530; height: Math.min(710, win.height - 60)
        title: qsTr("Preparar fotografías de graduación")
        contentItem: ScrollView {
            id: helpScroll
            contentWidth: availableWidth; clip: true
            ScrollBar.vertical: AppScrollBar {}
            Label {
                color: colors.text
                width: helpScroll.availableWidth; wrapMode: Text.WordWrap
                text: qsTr("1. Agrega fotos o carga una carpeta.\n2. Ajusta los tres tamaños con los controles o arrastrando la vista del encuadre. La rueda amplía.\n3. Aplica los encuadres a todo el lote y revisa las excepciones.\n4. Elige una carpeta de salida y exporta.\n\nCada foto produce una hoja de 8 × 10 pulgadas a 300 ppp, con 7 copias. Los nombres repetidos reciben un sufijo. JPG y PNG contienen la resolución de impresión; el PDF conserva el tamaño de página.\n\nCtrl+O: agregar fotos\nCtrl+Shift+O: cargar carpeta\nCtrl+Z / Ctrl+Shift+Z: deshacer / rehacer\nCtrl+Enter o Espacio: exportar\n?: ayuda    Q: cerrar\n\nAl imprimir, selecciona tamaño real / 100 %, sin ajustar a página. Usa papel de 8 × 10 pulgadas e impresión sin bordes para conservar todas las medidas.")
            }
        }
        footer: AppDialogButtons { ActionButton { text: qsTr("Entendido"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole } }
    }
    AppDialog {
        id: exportProgressDialog
        objectName: "exportProgressDialog"
        anchors.centerIn: Overlay.overlay
        modal: true; focus: true; closePolicy: Popup.NoAutoClose
        visible: backend.exporting
        width: Math.min(520, win.width - 60)
        title: backend.cancelRequested ? qsTr("Cancelando exportación…") : qsTr("Exportando fotografías")
        contentItem: ColumnLayout {
            spacing: 16
            BusyIndicator {
                objectName: "exportSpinner"
                Layout.alignment: Qt.AlignHCenter
                running: backend.exporting
                palette.dark: colors.accent
                implicitWidth: 56; implicitHeight: 56
            }
            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: qsTr("%1 de %2 fotografías procesadas").arg(backend.exportProcessed).arg(backend.exportTotal)
                color: colors.text; font.pixelSize: 16; font.weight: Font.DemiBold
            }
            AppProgressBar {
                objectName: "exportProgressBar"
                Layout.fillWidth: true; Layout.preferredHeight: 9
                value: backend.progress
            }
            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: Math.floor(backend.progress * 100) + " %"
                color: colors.text; font.pixelSize: 14
            }
            SmallLabel {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: backend.exportFile.length ? backend.exportFile : qsTr("Preparando el lote…")
                textFormat: Text.PlainText
                elide: Text.ElideMiddle; wrapMode: Text.NoWrap
            }
            SmallLabel {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: backend.cancelRequested ? qsTr("Terminando el archivo actual antes de detener el lote.")
                    : qsTr("Puedes cancelar. Los archivos terminados se conservarán.")
            }
        }
        footer: AppDialogButtons {
            ActionButton {
                objectName: "cancelExportButton"
                text: backend.cancelRequested ? qsTr("Cancelando…") : qsTr("Cancelar exportación")
                enabled: !backend.cancelRequested
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: backend.cancel()
            }
        }
    }
    AppDialog {
        id: exportResultDialog
        objectName: "exportResultDialog"
        anchors.centerIn: Overlay.overlay
        modal: true; focus: true
        width: Math.min(560, win.width - 60)
        title: backend.exportOutcome === "cancelled" ? qsTr("Exportación cancelada")
            : backend.exportOutcome === "error" ? qsTr("No se pudo exportar el lote")
            : backend.exportOutcome === "partial" ? qsTr("Exportación terminada con errores") : qsTr("Exportación completada")
        contentItem: ColumnLayout {
            spacing: 14
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: qsTr("Archivos exportados: %1 · Errores: %2").arg(backend.exportFiles).arg(backend.exportFailed)
                color: backend.exportOutcome === "success" ? colors.success
                    : backend.exportOutcome === "error" ? colors.error : colors.warningText
                font.pixelSize: 17; font.weight: Font.DemiBold
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: backend.exportOutcome === "cancelled"
                    ? qsTr("El lote se detuvo. Los archivos terminados se conservaron; puedes continuar con Solo pendientes.")
                    : backend.exportFailed > 0 ? qsTr("Revisa los detalles de los archivos que no se pudieron exportar.")
                    : qsTr("Las fotografías están listas en la carpeta de salida.")
                color: colors.text
            }
            SmallLabel {
                Layout.fillWidth: true; text: backend.destination
                textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere
            }
        }
        footer: AppDialogButtons {
            ActionButton {
                text: qsTr("Ver detalles"); visible: backend.exportFailed > 0
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: { exportResultDialog.close(); detailsDialog.open() }
            }
            ActionButton {
                text: qsTr("Abrir carpeta"); enabled: backend.exportFiles > 0
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: { backend.openDestination(); exportResultDialog.close() }
            }
            ActionButton {
                objectName: "dismissExportResult"
                text: qsTr("Aceptar"); primary: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }

}
