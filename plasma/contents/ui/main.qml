import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
PlasmoidItem {
 id: applet
 Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
 Layout.minimumWidth: 240
 Layout.minimumHeight: 170
 switchWidth: 0; switchHeight: 0
 preferredRepresentation: fullRepresentation
 fullRepresentation: Loader {
  id: renderer
  source: Qt.resolvedUrl("ClothView-v"+Plasmoid.configuration.rendererRevision+".qml")
  onLoaded: item.initialize(applet)
 }
}
