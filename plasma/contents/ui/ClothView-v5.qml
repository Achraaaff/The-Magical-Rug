import QtQuick
import QtQuick.Controls as Controls
import org.kde.plasma.plasmoid
import "native-v5" as Native
Item {
 id: view
 property var appletRoot: null
 property var container: null
 property alias cloth: cloth
 function initialize(root) {
  appletRoot=root
  initializeTimer.start()
 }
 function attach() {
  let item=appletRoot.parent
  for(let n=0;item && n<16;++n,item=item.parent) {
   if(item.applet!==undefined && item.layout!==undefined) {
    container=item
    item.layout.releaseSpace(item)
    cloth.attach(item)
    item.containmentMask=desktopMask
    cloth.setup(Plasmoid.configuration.rugX,Plasmoid.configuration.rugY,Plasmoid.configuration.rugWidth,Plasmoid.configuration.rugHeight,Plasmoid.configuration.clothState)
    console.info("DesktopRug native renderer ready",cloth.diagnostics())
    return
   }
  }
 }
 Timer {id: initializeTimer;interval:300;onTriggered:view.attach()}
 QtObject {
  id:desktopMask
  function contains(point: point): bool {
   if(!view.container || cloth.locked) return false
   const p=cloth.mapFromItem(view.container,point.x,point.y)
   if(toolbar.visible) {
    const t=toolbar.mapFromItem(cloth,p.x,p.y)
    if(t.x>=0 && t.y>=0 && t.x<toolbar.width && t.y<toolbar.height) return true
   }
   return cloth.hit(p.x,p.y)
  }
 }
 Native.ClothSurface {
  id:cloth
  objectName:"cloth"
  enabled:!locked
  anchors.fill:parent
  source:Qt.resolvedUrl("../assets/persian.jpg")
  Accessible.name:"Desktop Rug"
  Accessible.description:"Drag the middle to move instantly. Drag any edge to lift and fold the cloth. Control-drag to resize. Scroll or Shift-drag to rotate. Double-click to fold or flatten."
  onStateSaved: {
   Plasmoid.configuration.clothState=cloth.stateJson
   Plasmoid.configuration.engineDiagnostics=cloth.diagnostics()
   console.info("DesktopRug native settled",cloth.diagnostics())
  }
 }
 Rectangle {
  id:toolbar
  z:100
  width:234;height:36;radius:18;color:"#ef22252d";border.color:"#667b7367"
  x:Math.max(0,Math.min(view.width-width,cloth.clothBounds.x+(cloth.clothBounds.width-width)/2))
  y:Math.max(0,Math.min(view.height-height,cloth.clothBounds.y+cloth.clothBounds.height-height-12))
  readonly property bool hovered:hoverArea.containsMouse||foldButton.hovered||leftButton.hovered||rightButton.hovered||smallButton.hovered||largeButton.hovered
  visible:!cloth.locked&&(cloth.hovered||hovered)&&!cloth.interacting
  MouseArea {id:hoverArea;anchors.fill:parent;hoverEnabled:true;acceptedButtons:Qt.NoButton}
  Controls.ToolButton {id:foldButton;x:4;width:82;height:36;text:"Fold / flat";onClicked:cloth.toggleFold();Accessible.name:"Fold or flatten rug"}
  Controls.ToolButton {id:leftButton;x:86;width:34;height:36;text:"↶";onClicked:cloth.turn(-5);Accessible.name:"Rotate rug left"}
  Controls.ToolButton {id:rightButton;x:120;width:34;height:36;text:"↷";onClicked:cloth.turn(5);Accessible.name:"Rotate rug right"}
  Controls.ToolButton {id:smallButton;x:154;width:36;height:36;text:"−";onClicked:cloth.resizeRug(.94);Accessible.name:"Make rug smaller"}
  Controls.ToolButton {id:largeButton;x:190;width:36;height:36;text:"+";onClicked:cloth.resizeRug(1.06);Accessible.name:"Make rug larger"}
 }
}
