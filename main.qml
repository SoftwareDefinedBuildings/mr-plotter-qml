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

        //a1.addStream(s1);
        //a2.addStream(s2);
        //a3.addStream(s3);

        a1.name = "Axis 1";
        a2.name = "Axis 2";
        a3.name = "Axis 3";

        a2.dynamicAutoscale = true;

        var dds = mrp.newStream("ff51541a-d71c-47bc-bd63-fbbe68f94acd");
        dds.setColor(0, 0, 1.0);
        var dda = mrp.newYAxis(0, 10);
        dda.dynamicAutoscale = true;
        dda.setMinTicks(2);
        //dda.addStream(dds);

        var streamlist = [s1, s2, s3];
        var axislist = [a1, a2, a3];

        //pa.setStreamList(streamlist);
        //ddpa.setStreamList([dds]);
        yaa.setAxisList(axislist);
        ddyaa.setAxisList([dda]);

        mrp.setTimeDomain(1415643674979, 1415643674979, 469055, 469318);
        mrp.setTimeZone("America/Los_Angeles");

        pa.update();
        yaa.update();

        mrp.updateDataAsync();
    }

    PlotArea {
        id: ddpa
        anchors.left: pa.left
        width: pa.width
        height: 70
        yaxisarea: ddyaa
    }

    YAxisArea {
        id: ddyaa
        anchors.top: ddpa.top
        anchors.right: ddpa.left
        width: yaa.width
        height: ddpa.height
    }

    PlotArea {
        id: pa
        y: ddpa.height + 8
        anchors.left: yaa.right
        width: parent.width - yaa.width
        height: Math.max(parent.height - taa.height - ddpa.height, 60)
        yaxisarea: yaa
    }

    YAxisArea {
        id: yaa
        anchors.top: pa.top
        anchors.left: parent.left
        width: Math.min(parent.width / 4, 300)
        height: pa.height
    }

    TimeAxisArea {
        id: taa
        anchors.top: pa.bottom
        x: pa.x
        width: pa.width
        height: 70
    }

    MrPlotter {
        id: mrp
        mainPlot: pa
        dataDensityPlot: ddpa
        timeaxisarea: taa
    }
}
