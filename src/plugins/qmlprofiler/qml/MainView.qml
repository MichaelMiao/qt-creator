/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at info@qt.nokia.com.
**
**************************************************************************/

import QtQuick 1.0
import Monitor 1.0
import "MainView.js" as Plotter

Rectangle {
    id: root

    property variant colors:  Plotter.colors  //the colors used for the timeline data
    property bool xRay: false                 //useful for seeing "nested" ranges (but redraw is buggy -- QGV problem?)
    property Item currentItem                 //currently selected item in the view

    // move the cursor in the editor
    signal updateCursorPosition
    property string fileName: ""
    property int lineNumber: -1
    function gotoSourceLocation(file,line) {
        root.fileName = file;
        root.lineNumber = line;
        root.updateCursorPosition();
    }

    function clearData() {
        Plotter.reset();
        view.clearData();
        rangeMover.x = 2
        rangeMover.opacity = 0
    }

    function clearAll() {
        clearData();
        selectedEventIndex = -1;
        Plotter.valuesdone = false;
        canvas.requestPaint();
        view.visible = false;
        root.elapsedTime = 0;
        root.updateTimer();
    }

    property int selectedEventIndex : -1;
    property bool mouseTracking: false;

    //handle debug data coming from C++
    Connections {
        target: connection
        onEvent: {
            if (Plotter.valuesdone) {
                root.clearData();
            }

            if (!Plotter.valuesdone && event === 0) //### only handle paint event
                Plotter.values.push(time);
        }

        onRange: {
            if (Plotter.valuesdone) {
                root.clearData();
            }

            // todo: consider nestingLevel
            if (!Plotter.valuesdone)
                Plotter.ranges.push( { type: type, start: startTime, duration: length, label: data, fileName: fileName, line: line } );
        }

        onComplete: {
            Plotter.valuesdone = true;
            Plotter.calcFps();
            view.visible = true;
            view.setRanges(Plotter.ranges);
            view.updateTimeline();
            canvas.requestPaint();
            rangeMover.x = 1    //### hack to get view to display things immediately
            rangeMover.x = 0
            rangeMover.opacity = 1
        }

        onClear: {
            root.clearAll();
        }

    }

    // Elapsed
    property real elapsedTime;
    signal updateTimer;
    Timer {
        property date startDate
        property bool reset:  true
        running: connection.recording
        repeat: true
        onRunningChanged: if (running) reset = true
        interval:  100
        triggeredOnStart: true
        onTriggered: {
            if (reset) {
                startDate = new Date()
                reset = false
            }
            var time = (new Date() - startDate)/1000
            //elapsed.text = time.toFixed(1) + "s"
            root.elapsedTime = time.toFixed(1);
            root.updateTimer();
        }
    }

    // time markers
    TimeDisplay {
        height: flick.height + labels.y
        anchors.left: flick.left
        anchors.right: flick.right
        startTime: rangeMover.x * Plotter.xScale(canvas);
        endTime: (rangeMover.x + rangeMover.width) * Plotter.xScale(canvas);
    }

    function hideRangeDetails() {
        rangeDetails.visible = false
        rangeDetails.duration = ""
        rangeDetails.label = ""
        rangeDetails.type = ""
        rangeDetails.file = ""
        rangeDetails.line = -1
    }

    //our main interaction view
    Flickable {
        id: flick
        anchors.top: parent.top
        anchors.topMargin: labels.y
        anchors.right: parent.right
        anchors.left: labels.right
        anchors.bottom: canvas.top
        contentWidth: view.totalWidth
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick

        clip:true

        MouseArea {
            width: flick.width
            height: flick.contentHeight
            x:  flick.contentX
            onClicked: {
                root.hideRangeDetails();
            }
            hoverEnabled: true
            onExited: {
                root.hideRangeDetails();
            }
        }

        TimelineView {
            id: view

            width: flick.width;
            height: flick.contentHeight;

            startX: flick.contentX
            onStartXChanged: {
                var newX = startTime / Plotter.xScale(canvas) - canvas.canvasWindow.x;
                if (Math.abs(rangeMover.x - newX) > .01)
                    rangeMover.x = newX
                if (Math.abs(startX - flick.contentX) > .01)
                    flick.contentX = startX
            }
            startTime: rangeMover.value

            property real prevXStep:  -1
            property real possibleEndTime: startTime + (rangeMover.width*Plotter.xScale(canvas))
            onPossibleEndTimeChanged:  {
                prevXStep = canvas.canvasWindow.x;
                endTime = possibleEndTime
            }
            onEndTimeChanged: updateTimeline()

            delegate: Rectangle {
                id: obj

                property color baseColor: colors[type]
                property color myColor: baseColor

                function conditionalHide() {
                    if (!mouseArea.containsMouse)
                        mouseArea.exited()
                }

                height: view.height / labels.rowCount
                y: type * view.height / labels.rowCount;
                gradient: Gradient {
                    GradientStop { position: 0.0; color: myColor }
                    GradientStop { position: 0.5; color: Qt.darker(myColor, 1.1) }
                    GradientStop { position: 1.0; color: myColor }
                }
                smooth: true

                property bool componentIsCompleted: false
                Component.onCompleted: componentIsCompleted = true;

                property bool isSelected: root.selectedEventIndex == index;
                onIsSelectedChanged: {
                    if (!root.mouseTracking && componentIsCompleted) {
                        if (isSelected) {
                            enableSelected(0, 0);
                        }
                        else
                            disableSelected();
                    }
                }

                function enableSelected(x,y) {
                    currentItem = obj
                    myColor = Qt.darker(baseColor, 1.2)
                    rangeDetails.duration = duration
                    rangeDetails.label = label
                    rangeDetails.file = fileName
                    rangeDetails.line = line
                    rangeDetails.type = Plotter.names[type]

                    var pos = mapToItem(rangeDetails.parent, x, y+height)
                    var preferredX = Math.max(10, pos.x - rangeDetails.width/2)
                    if (preferredX + rangeDetails.width > rangeDetails.parent.width)
                        preferredX = rangeDetails.parent.width - rangeDetails.width
                    rangeDetails.x = preferredX

                    var preferredY = pos.y - rangeDetails.height/2;
                    if (preferredY + rangeDetails.height > root.height - 10)
                        preferredY = root.height - 10 - rangeDetails.height;
                    if (preferredY < 10)
                        preferredY=10;
                    rangeDetails.y = preferredY;
                    rangeDetails.visible = true
                }

                function disableSelected() {
                    myColor = baseColor
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        root.mouseTracking = true;
                        root.selectedEventIndex = index;
                        enableSelected(mouseX, y);
                        root.mouseTracking = false;
                    }
                    onExited: {
                        disableSelected();
                    }

                    onClicked: root.gotoSourceLocation(rangeDetails.file, rangeDetails.line);
                }
            }
        }
    }

    //popup showing the details for the hovered range
    RangeDetails {
        id: rangeDetails

        // follow the flickable
        property int flickableX: flick.contentX;
        property int lastFlickableX;
        onXChanged: lastFlickableX = flickableX;
        onFlickableXChanged: {
            x = x - flickableX + lastFlickableX;
            if (visible && (x + width <= 0 || x > root.width)) {
                root.hideRangeDetails();
                visible = false;
            }
        }
    }

    Rectangle {
        id: labels
        width: 150
        color: "#dcdcdc"
        y: 12
        height: flick.height

        property int rowCount: 5

        Column {
            id: col
            //### change to use Repeater + Plotter.names?
            Label { text: "Painting"; height: labels.height/labels.rowCount}
            Label { text: "Compiling"; height: labels.height/labels.rowCount }
            Label { text: "Creating"; height: labels.height/labels.rowCount }
            Label { text: "Binding"; height: labels.height/labels.rowCount }
            Label { text: "Signal Handler"; height: labels.height/labels.rowCount }
        }

        //right border divider
        Rectangle {
            width: 1
            height: parent.height
            anchors.right: parent.right
            color: "#cccccc"
        }
    }

    //bottom border divider
    Rectangle {
        height: 1
        width: parent.width
        anchors.bottom: canvas.top
        color: "#cccccc"
    }

    //"overview" graph at the bottom
    TiledCanvas {
        id: canvas

        anchors.bottom: parent.bottom
        width: parent.width; height: 50

        property int canvasWidth: width

        canvasSize {
            width: canvasWidth
            height: canvas.height
        }

        tileSize.width: width
        tileSize.height: height

        canvasWindow.width: width
        canvasWindow.height: height

        onDrawRegion: {
             if (Plotter.valuesdone)
                 Plotter.plot(canvas, ctxt, region);
             else
                 Plotter.drawGraph(canvas, ctxt, region)    //just draw the background
        }
    }

    //moves the range mover to the position of a click
    MouseArea {
        anchors.fill: canvas
        //### ideally we could press to position then immediately drag
        onPressed: rangeMover.x = mouse.x - rangeMover.width/2
    }

    RangeMover {
        id: rangeMover
        opacity: 0
        anchors.top: canvas.top
    }
}
