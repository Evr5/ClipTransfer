import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 420
    height: 780
    title: "ClipTransfer"

    StackLayout {
        anchors.fill: parent
        currentIndex: chatController.nickname.length === 0 ? 0 : 1

        // First-run screen: choose nickname once.
        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 14

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Bienvenue")
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Choisissez votre pseudo.")
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                TextField {
                    id: nicknameField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Pseudo")
                    inputMethodHints: Qt.ImhNoPredictiveText
                }

                Button {
                    Layout.fillWidth: true
                    text: qsTr("Valider")
                    enabled: nicknameField.text.trim().length > 0
                    onClicked: chatController.nickname = nicknameField.text
                }
            }
        }

        // Main chat screen.
        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Pseudo : %1").arg(chatController.nickname)
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("ClipTransfer – Chat LAN (UDP broadcast)")
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                TextArea {
                    id: historyArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    readOnly: true
                    wrapMode: TextEdit.NoWrap
                    text: chatController.history.join("\n")
                    onTextChanged: historyArea.cursorPosition = historyArea.length
                }

                TextArea {
                    id: inputArea
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    placeholderText: qsTr("Écrire un message...")
                    wrapMode: TextEdit.NoWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Envoyer presse-papiers")
                        enabled: chatController.running
                        onClicked: chatController.sendClipboard()
                    }
                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Envoyer texte")
                        enabled: chatController.running
                        onClicked: {
                            chatController.sendText(inputArea.text)
                            inputArea.text = ""
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Copier message complet")
                        onClicked: chatController.copyLastReceivedToClipboard()
                    }
                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Effacer conversation")
                        onClicked: chatController.clearHistory()
                    }
                }
            }
        }
    }
}
