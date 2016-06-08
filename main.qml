import QtQuick 2.6
import QtQuick.Window 2.2
import MrPlotter 0.1

Window {
    visible: true

    MainForm {
        anchors.fill: parent
        mouseArea.onClicked: {
            var s = x.addStream("91c0dd4c-8d88-4803-a0b9-3aa7cd09fa0f")
            console.log("Hello, world " + s.toString());
        }
    }

    PlotArea {
        id: pa
        width: parent.width
        height: parent.height / 2
        timeaxisarea: taa
    }

    TimeAxisArea {
        id: taa
        anchors.top: pa.bottom
        width: pa.width
        height: parent.height / 2
        MouseArea {
            anchors.fill: parent
            onClicked: {
                console.log("Clicked axis area");
            }
        }
    }

    MrPlotter {
        id: x
        plotarea: pa
    }
}
