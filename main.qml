import QtQuick 2.6
import QtQuick.Window 2.2
import MrPlotter 1.0

Window {
    visible: true

    MainForm {
        anchors.fill: parent
        mouseArea.onClicked: {
            //Qt.quit();
        }
    }

    PlotArea {
        anchors.fill: parent
    }
}
