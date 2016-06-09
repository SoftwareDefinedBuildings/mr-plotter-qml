import QtQuick 2.6
import QtQuick.Window 2.2
import MrPlotter 0.1

Window {
    visible: true

    MainForm {
        anchors.fill: parent
        mouseArea.onClicked: {
            var s = mrp.addStream("91c0dd4c-8d88-4803-a0b9-3aa7cd09fa0f")
            console.log("Hello, world " + s.toString());
        }
    }

    PlotArea {
        id: pa
        x: parent.width / 2
        width: parent.width / 2
        height: parent.height / 2
        timeaxisarea: taa
        yaxisarea: yaa
    }

    YAxisArea {
        id: yaa
        anchors.right: pa.left
        width: parent.width / 2
        height: pa.height
        MouseArea {
            anchors.fill: parent
            onClicked: {
                console.log("Clicked y axis area");
            }
        }
    }

    TimeAxisArea {
        id: taa
        anchors.top: pa.bottom
        x: pa.width
        width: pa.width
        height: parent.height / 2
        MouseArea {
            anchors.fill: parent
            onClicked: {
                console.log("Clicked time axis area");
            }
        }
    }

    MrPlotter {
        id: mrp
        plotarea: pa
    }
}
