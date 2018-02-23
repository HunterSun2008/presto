import QtQuick 2.7
import QtQuick.Controls 2.0 as QQC2
import QtQuick.Layouts 1.3
import org.kde.kirigami 2.1 as Kirigami

Kirigami.OverlaySheet {
    property string onchainAddress

    ColumnLayout {

        QQC2.Label {
            
            visible: !walletModel.totalAvailableFunds
            wrapMode: Text.WordWrap
            text: "NO FUNDS"
        }

        QQC2.Label {
            
            Layout.alignment: Qt.AlignCenter
            font.pixelSize: 22
            text: qsTr("Your On-Chain Address")
        }

        QRCode {
            Layout.alignment: Qt.AlignCenter
            width : 320
            height : 320
            value : "bitcoin:" + onchainAddress
        }

        QQC2.Label {
            
            Layout.alignment: Qt.AlignCenter
            wrapMode: Text.WordWrap
            font: fixedFont
            text: onchainAddress

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    clipboard.setText(onchainAddress)
                    showPassiveNotification(qsTr("Address Copied to Clipboard"))
                }
            }
        }
    }
}
