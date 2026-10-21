import QtQuick
import QtQuick.Window
import "../build/package/contents/ui/native-v5" as Native
Window {
 id: win
 width: 1100; height: 800; visible: true; color: "#283640"
 title: "Desktop Rug — isolated renderer test"
 Native.ClothSurface {
  enabled:!locked
  id: cloth; objectName: "cloth"; anchors.fill: parent
  source: Qt.resolvedUrl("../plasma/contents/assets/persian.jpg")
  Component.onCompleted: setup(150,120,750,500,"")
  onStateSaved: console.info("CLOTH_STATS",diagnostics())
 }

 Text {x: 20;y: 20;color: "white";text: "Native cloth test · drag an edge to lift · drag the middle to move · double-click to flatten"}
}
