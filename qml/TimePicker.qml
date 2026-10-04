// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Pick a time: any day, stepped back and on, and the hour and minutes, each
// with big steps to tap. Set keeps it.
ColumnLayout {
    id: picker

    property date chosen: new Date()

    // Where the choosing starts: the next whole hour.
    function reset() {
        const start = new Date();
        start.setHours(start.getHours() + 1, 0, 0, 0);
        chosen = start;
    }
    // The desktop's short time, to the minute.
    readonly property string timeFormat: Qt.locale().timeFormat(Locale.ShortFormat).replace(/[:.]ss/, "")

    signal picked(date time)

    function moved(days, minutes) {
        const next = new Date(chosen.getTime());
        next.setDate(next.getDate() + days);
        next.setMinutes(next.getMinutes() + minutes);
        const now = new Date();
        if (next > now) {
            chosen = next;
        }
    }

    function dayWords(day) {
        const today = new Date();
        today.setHours(0, 0, 0, 0);
        const that = new Date(day.getTime());
        that.setHours(0, 0, 0, 0);
        const days = Math.round((that - today) / 86400000);
        if (days === 0) return i18n("Today");
        if (days === 1) return i18n("Tomorrow");
        if (days < 7) return Qt.locale().toString(day, "dddd");
        return Qt.locale().toString(day, "ddd d MMMM");
    }

    spacing: 8

    RowLayout {
        spacing: 8
        Pill {
            objectName: "dayBack"
            text: "‹"
            Accessible.name: i18n("A day earlier")
            onClicked: picker.moved(-1, 0)
        }
        QQC2.Label {
            objectName: "pickedDay"
            Layout.preferredWidth: 170
            horizontalAlignment: Text.AlignHCenter
            text: picker.dayWords(picker.chosen)
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }
        Pill {
            objectName: "dayOn"
            text: "›"
            Accessible.name: i18n("A day later")
            onClicked: picker.moved(1, 0)
        }
    }

    RowLayout {
        spacing: 8
        Pill {
            objectName: "hourBack"
            text: "‹"
            Accessible.name: i18n("An hour earlier")
            onClicked: picker.moved(0, -60)
        }
        Pill {
            objectName: "minutesBack"
            text: "−15"
            Accessible.name: i18n("A quarter hour earlier")
            onClicked: picker.moved(0, -15)
        }
        QQC2.Label {
            objectName: "pickedTime"
            Layout.preferredWidth: 90
            horizontalAlignment: Text.AlignHCenter
            text: Qt.locale().toString(picker.chosen, picker.timeFormat)
            font.pixelSize: 20
            font.weight: Font.Bold
        }
        Pill {
            objectName: "minutesOn"
            text: "+15"
            Accessible.name: i18n("A quarter hour later")
            onClicked: picker.moved(0, 15)
        }
        Pill {
            objectName: "hourOn"
            text: "›"
            Accessible.name: i18n("An hour later")
            onClicked: picker.moved(0, 60)
        }
        Pill {
            objectName: "setTime"
            primary: true
            text: i18n("Set")
            onClicked: picker.picked(picker.chosen)
        }
    }
}
