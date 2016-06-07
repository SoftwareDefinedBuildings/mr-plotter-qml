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
            var t = new Stream();
            console.log(t);
        }
    }

    PlotArea {
        id: pa
        width: parent.width / 2
        height: parent.height / 2
    }

    MrPlotter {
        id: x
        plotarea: pa
    }
}
