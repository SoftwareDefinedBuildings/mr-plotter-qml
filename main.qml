import QtQuick 2.6
import QtQuick.Window 2.2
import MrPlotter 0.1

Window {
    visible: true

    MainForm {
        anchors.fill: parent
    }

    PlotArea {
        id: pa
        anchors.left: yaa.right
        width: parent.width - yaa.width
        height: Math.max(parent.height - taa.height, 50)
        timeaxisarea: taa
        yaxisarea: yaa
    }

    YAxisArea {
        id: yaa
        anchors.left: parent.left
        width: Math.min(parent.width / 4, 300)
        height: pa.height
    }

    TimeAxisArea {
        id: taa
        anchors.top: pa.bottom
        x: pa.x
        width: pa.width
        height: 30
    }

    MrPlotter {
        id: mrp
        plotarea: pa
    }
}
