import QtQuick 2.6
import QtQuick.Window 2.2
import MrPlotter 0.1

Window {
    visible: true

    Component.onCompleted: {
        var s1 = mrp.newStream("ff51541a-d71c-47bc-bd63-fbbe68f94acd");
        var s2 = mrp.newStream("ff51541a-d71c-47bc-bd63-fbbe68f94acd");
        var s3 = mrp.newStream("ff51541a-d71c-47bc-bd63-fbbe68f94acd");

        var a1 = mrp.newYAxis(-2, 2);
        var a2 = mrp.newYAxis(-10, 2);
        var a3 = mrp.newYAxis(-2, 10);

        s1.setColor(0, 0, 1.0);
        s2.setColor(1.0, 0, 0);
        s3.setColor(0, 0.5, 0);

        a1.addStream(s1);
        a2.addStream(s2);
        a3.addStream(s3);

        var streamlist = [s1, s2, s3];
        var axislist = [a1, a2, a3];

        pa.setStreamList(streamlist);
        yaa.setAxisList(axislist);

        pa.setTimeDomain(1415643674999, 1415643675001);

        pa.update();
        yaa.update();

        pa.updateDataAsync();
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
