#include "cloth.hpp"
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGMaterial>
#include <QtQuick/QSGMaterialShader>
#include <QtQuick/QSGTexture>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQml/qqml.h>
#include <QtCore/QTimer>
#include <QtDBus/QDBusConnection>
#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtGui/QMouseEvent>
#include <QtGui/QImageReader>
#include <QtGui/QHoverEvent>
#include <QtGui/QWheelEvent>
#include <numeric>
#include <cstring>
static QString shaderPath;
struct Vertex {float x,y,u,v,nx,ny,nz,z;};
class RugMaterial;
class RugShader: public QSGMaterialShader {
public:
 RugShader(){setShaderFileName(VertexStage,shaderPath+"/cloth.vert.qsb");setShaderFileName(FragmentStage,shaderPath+"/cloth.frag.qsb");}
 bool updateUniformData(RenderState&,QSGMaterial*,QSGMaterial*)override;
 void updateSampledImage(RenderState&,int,QSGTexture**,QSGMaterial*,QSGMaterial*)override;
};
class RugMaterial:public QSGMaterial{
public:
 QSGTexture* texture=nullptr;float shadow=0;QSizeF size;
 RugMaterial(){setFlag(Blending,true);setFlag(RequiresFullMatrix,true);}
 QSGMaterialType* type()const override{static QSGMaterialType t;return &t;}
 QSGMaterialShader* createShader(QSGRendererInterface::RenderMode)const override{return new RugShader;}
 int compare(const QSGMaterial*other)const override{auto o=static_cast<const RugMaterial*>(other);if(shadow!=o->shadow)return shadow<o->shadow?-1:1;if(texture==o->texture)return 0;return std::less<QSGTexture*>{}(texture,o->texture)?-1:1;}
};
bool RugShader::updateUniformData(RenderState&s,QSGMaterial* m,QSGMaterial*){
 auto mat=static_cast<RugMaterial*>(m);auto* b=s.uniformData();if(b->size()<80)b->resize(80);
 std::memcpy(b->data(),s.combinedMatrix().constData(),64);float opacity=s.opacity();std::memcpy(b->data()+64,&opacity,4);
 std::memcpy(b->data()+68,&mat->shadow,4);float dims[2]={float(mat->size.width()),float(mat->size.height())};std::memcpy(b->data()+72,dims,8);return true;
}
void RugShader::updateSampledImage(RenderState&s,int binding,QSGTexture**t,QSGMaterial*m,QSGMaterial*){
 if(binding!=1)return;auto*texture=static_cast<RugMaterial*>(m)->texture;if(texture)texture->commitTextureOperations(s.rhi(),s.resourceUpdateBatch());*t=texture;
}
class RootNode:public QSGNode{
public:QSGTexture* texture=nullptr;~RootNode()override{delete texture;}
};
class ClothSurface:public QQuickItem{
 Q_OBJECT
 Q_CLASSINFO("D-Bus Interface", "org.local.DesktopRug")
 Q_PROPERTY(bool locked READ locked WRITE setLocked NOTIFY lockedChanged)
 Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
 Q_PROPERTY(QRectF clothBounds READ clothBounds NOTIFY boundsChanged)
 Q_PROPERTY(bool hovered READ hovered NOTIFY hoveredChanged)
 Q_PROPERTY(bool interacting READ interacting NOTIFY interactingChanged)
 Q_PROPERTY(bool sleeping READ sleeping NOTIFY sleepingChanged)
 Q_PROPERTY(double simulationMs READ simulationMs NOTIFY statsChanged)
 Q_PROPERTY(QString stateJson READ stateJson WRITE setStateJson NOTIFY stateSaved)
 Cloth sim;QUrl imageSource;QImage image;QTimer timer;QElapsedTimer elapsed;std::vector<QPointF> projected;QRectF bounds;
 bool isHovered=false,active=false,dirtyImage=true,folded=false,isLocked=false,ownsControlPath=false;int mode=0,frames=0,quiet=0;QPointF initialMouse,previousMouse;float initialScale=1;double ms=0;QString pendingState;
 QSizeF initialRug;
 float initialGrabHeight=0;
 V3 grabPointerOffset;
public:
 ClothSurface(){
  setFlag(ItemHasContents,true);setAcceptedMouseButtons(Qt::LeftButton);setAcceptHoverEvents(true);setKeepMouseGrab(true);
  sim.reset(322,20,800,520);derive();timer.setTimerType(Qt::PreciseTimer);timer.setInterval(16);
  connect(&timer,&QTimer::timeout,this,[this]{tick();});
  ownsControlPath=QDBusConnection::sessionBus().registerObject("/org/local/DesktopRug",this,QDBusConnection::ExportScriptableSlots);
 }
 ~ClothSurface()override{if(ownsControlPath)QDBusConnection::sessionBus().unregisterObject("/org/local/DesktopRug");}
 bool locked()const{return isLocked;}
 void setLocked(bool value){
  if(value==isLocked)return;
  isLocked=value;
  if(value){
   timer.stop();sim.end();for(auto &v:sim.vel)v={};mode=0;
   ungrabMouse();isHovered=false;unsetCursor();
  }
  setAcceptHoverEvents(!value);setAcceptedMouseButtons(value?Qt::NoButton:Qt::LeftButton);
  emit lockedChanged();emit hoveredChanged();emit interactingChanged();emit sleepingChanged();emit stateSaved();
 }
public slots:
 Q_SCRIPTABLE void ToggleLock(){setLocked(!isLocked);}
 Q_SCRIPTABLE bool IsLocked()const{return isLocked;}
public:
 QUrl source()const{return imageSource;}
 void setSource(QUrl s){if(s==imageSource)return;imageSource=s;QImageReader r(s.toLocalFile());QSize size=r.size();if(size.width()>1536){size.scale(1536,1536,Qt::KeepAspectRatio);r.setScaledSize(size);}image=r.read();dirtyImage=true;update();emit sourceChanged();}
 QRectF clothBounds()const{return bounds;}bool hovered()const{return isHovered;}bool interacting()const{return mode!=0;}bool sleeping()const{return !timer.isActive();}double simulationMs()const{return ms;}
 QString stateJson()const{
  QJsonArray a;for(auto p:sim.pos){a.append(std::round(p.x*100)/100);a.append(std::round(p.y*100)/100);a.append(std::round(p.z*100)/100);}
  QJsonObject o{{"version",2},{"width",sim.rw},{"height",sim.rh},{"nx",sim.nx},{"ny",sim.ny},{"folded",folded},{"locked",isLocked},{"positions",a}};
  return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
 }
 void setStateJson(QString s){
  if(s.isEmpty())return;auto d=QJsonDocument::fromJson(s.toUtf8());if(!d.isObject())return;auto o=d.object();auto a=o["positions"].toArray();
  float w=o["width"].toDouble(),h=o["height"].toDouble();if(w<240||w>4000||h<140||h>3000)return;
  sim.reset(0,0,w,h);if(a.size()!=sim.count()*3)return;
  for(int i=0;i<sim.count();i++){V3 p{float(a[i*3].toDouble()),float(a[i*3+1].toDouble()),float(a[i*3+2].toDouble())};if(!std::isfinite(p.x+p.y+p.z)||std::abs(p.x)>10000||std::abs(p.y)>10000||p.z<0||p.z>1500)return;sim.pos[i]=p;sim.prev[i]=p;sim.vel[i]={};}
  folded=o["folded"].toBool();bool recovered=false;
  // Recover a mesh saved by the old fixed-contact engine rather than
  // preserving the stretched strips from that broken state.
  if(sim.maxStructuralStretch()>1.35f){
   auto c=sim.center();float x=std::clamp(c.x-sim.rw/2,2.f,std::max(2.f,float(width())-sim.rw-2));
   float y=std::clamp(c.y-sim.rh/2,2.f,std::max(2.f,float(height())-sim.rh-2));
   sim.reset(x,y,sim.rw,sim.rh);folded=false;recovered=true;
  }
  sim.derive();derive();update();setLocked(o["locked"].toBool());if(recovered)emit stateSaved();
 }
 Q_INVOKABLE void setup(float x,float y,float w,float h,const QString& saved){
  sim.reset(x,y,std::clamp(w,300.f,1600.f),std::clamp(h,200.f,1000.f));if(!saved.isEmpty())setStateJson(saved);derive();update();
 }
 Q_INVOKABLE void attach(QObject*object){
  auto*host=qobject_cast<QQuickItem*>(object);if(!host||!host->parentItem())return;auto*layout=host->parentItem();
  // Calling the native setters bypasses QML's ShortDropBehavior on x/y.
  // The actual cloth stays in this desktop scene; its positions are not animated applet coordinates.
  host->setRotation(0);host->setPosition({0,0});host->setSize(layout->size());
  connect(layout,&QQuickItem::widthChanged,this,[this,host,layout]{host->setWidth(layout->width());});
  connect(layout,&QQuickItem::heightChanged,this,[this,host,layout]{host->setHeight(layout->height());});
 }
 Q_INVOKABLE bool hit(qreal x,qreal y)const{return !isLocked&&pick({x,y})>=0;}
 bool contains(const QPointF&p)const override{return hit(p.x(),p.y());}
 Q_INVOKABLE void toggleFold(){
  if(isLocked)return;
  if(folded){flatten();return;}sim.foldCorner();folded=true;derive();update();wake();
 }
 Q_INVOKABLE void flatten(){
  if(isLocked)return;
  auto c=sim.center();sim.reset(c.x-sim.rw/2,c.y-sim.rh/2,sim.rw,sim.rh);folded=false;derive();update();timer.stop();emit sleepingChanged();emit stateSaved();
 }
 Q_INVOKABLE void turn(float degrees){if(isLocked)return;sim.rotate(degrees*3.14159265f/180);derive();update();emit stateSaved();}
 Q_INVOKABLE void resizeRug(float factor){if(isLocked)return;if(sim.rw*factor<300||sim.rw*factor>width()*.95||sim.rh*factor>height()*.95)return;sim.scale(factor);derive();update();emit stateSaved();}
 Q_INVOKABLE void stopSimulation(){timer.stop();sim.end();mode=0;emit sleepingChanged();emit interactingChanged();emit stateSaved();}
 Q_INVOKABLE QString diagnostics()const{
  return QString::fromUtf8(QJsonDocument(QJsonObject{{"edgeControlRevision",4},{"rendererRevision",5},{"locked",isLocked},{"maxStretch",sim.maxStructuralStretch()},{"particles",sim.count()},{"triangles",int(sim.faces.size())},{"physicsMs",ms},{"sleeping",sleeping()},{"folded",folded},{"mode",mode},{"x",bounds.x()},{"y",bounds.y()},{"width",bounds.width()},{"height",bounds.height()}}).toJson(QJsonDocument::Compact));
 }
signals:
 void lockedChanged();void sourceChanged();void boundsChanged();void hoveredChanged();void interactingChanged();void sleepingChanged();void statsChanged();void stateSaved();
protected:
 QPointF project(V3 p)const{
  float d=std::max(width(),height())*1.5f;float f=d/std::max(d-p.z,d*.4f);QPointF c(width()/2,height()/2);return c+(QPointF(p.x,p.y)-c)*f;
 }
 V3 unproject(QPointF p,float z)const{float d=std::max(width(),height())*1.5f,f=(d-z)/d;QPointF c(width()/2,height()/2);p=c+(p-c)*f;return {float(p.x()),float(p.y()),z};}
 void derive(){
  projected.resize(sim.count());float loX=1e9f,loY=1e9f,hiX=-1e9f,hiY=-1e9f;
  for(int i=0;i<sim.count();i++){auto p=project(sim.pos[i]);projected[i]=p;loX=std::min(loX,float(p.x()));loY=std::min(loY,float(p.y()));hiX=std::max(hiX,float(p.x()));hiY=std::max(hiY,float(p.y()));}
  bounds={loX,loY,hiX-loX,hiY-loY};emit boundsChanged();
 }
 int pick(QPointF p)const{
  if(!bounds.adjusted(-1,-1,1,1).contains(p))return -1;float best=-1e20f;int found=-1;
  for(auto f:sim.faces){QPointF a=projected[f[0]],b=projected[f[1]],c=projected[f[2]];
   if(p.x()<std::min({a.x(),b.x(),c.x()})||p.x()>std::max({a.x(),b.x(),c.x()})||p.y()<std::min({a.y(),b.y(),c.y()})||p.y()>std::max({a.y(),b.y(),c.y()}))continue;
   QPointF v0=b-a,v1=c-a,v2=p-a;double den=v0.x()*v1.y()-v1.x()*v0.y();if(std::abs(den)<1e-8)continue;
   double u=(v2.x()*v1.y()-v1.x()*v2.y())/den,v=(v0.x()*v2.y()-v2.x()*v0.y())/den,w=1-u-v;if(u<0||v<0||w<0)continue;
   float z=sim.pos[f[0]].z*w+sim.pos[f[1]].z*u+sim.pos[f[2]].z*v;
   if(z>best){best=z;found=w>=u&&w>=v?f[0]:u>=v?f[1]:f[2];}
  }return found;
 }
 void wake(){frames=0;quiet=0;elapsed.restart();if(!timer.isActive()){timer.start();emit sleepingChanged();}}
 void tick(){
  if(isLocked){timer.stop();return;}
  float dt=std::clamp(elapsed.restart()/1000.f,1.f/120,1.f/30);QElapsedTimer t;t.start();sim.step(dt);ms=t.nsecsElapsed()/1e6;
  if(!std::isfinite(sim.maxSpeed)){flatten();return;}
  derive();update();frames++;if(frames%20==0)emit statsChanged();
  if(!mode){if(sim.rmsSpeed<6)quiet++;else quiet=0;if(quiet>16||(frames>240&&sim.rmsSpeed<35)){timer.stop();for(auto&v:sim.vel)v={};emit sleepingChanged();emit stateSaved();}}
 }
 void mousePressEvent(QMouseEvent*e)override{
  if(isLocked){e->ignore();return;}
  int v=pick(e->position());if(v<0){e->ignore();return;}initialMouse=previousMouse=e->position();initialRug={sim.rw,sim.rh};
  int ix=v%sim.nx,jy=v/sim.nx;bool edge=ix<2||ix>=sim.nx-2||jy<2||jy>=sim.ny-2;
  if(e->modifiers()&Qt::ControlModifier){mode=3;}
  else if(e->modifiers()&Qt::ShiftModifier){mode=4;}
  else if(edge){
   mode=2;initialGrabHeight=std::max(0.f,sim.pos[v].z);
   // Pressing a corner is not an instruction to lift it 120 pixels.
   // Lift grows gently with the actual drag distance instead.
   grabPointerOffset=sim.pos[v]-unproject(e->position(),initialGrabHeight);
   sim.begin(v,sim.pos[v]);wake();
  }
  else {mode=1;sim.end();timer.stop();}
  setCursor(mode==2?Qt::ClosedHandCursor:mode==3?Qt::SizeFDiagCursor:Qt::ClosedHandCursor);emit interactingChanged();e->accept();
 }
 void mouseMoveEvent(QMouseEvent*e)override{
  if(isLocked){e->ignore();return;}
  QPointF p=e->position(),delta=p-previousMouse;previousMouse=p;
  if(mode==1){
   float dx=std::clamp(float(delta.x()),float(-bounds.left()+2),float(width()-bounds.right()-2));float dy=std::clamp(float(delta.y()),float(-bounds.top()+2),float(height()-bounds.bottom()-2));
   sim.translate(dx,dy);derive();update();
  }else if(mode==2){
   float distance=float(std::hypot(p.x()-initialMouse.x(),p.y()-initialMouse.y()));
   // Lift over the base first; lower the tip only after it has crossed
   // the crease. A low, constant handle height tucks the flap underneath.
   float peak=std::min(sim.rw,sim.rh)*.205f;
   float arc=std::min(distance*.9f,std::max(sim.thickness*1.5f,peak-std::max(0.f,distance-peak*1.36f)*.65f));
   float lift=initialGrabHeight+arc;
   sim.grabTarget=unproject(p,lift)+grabPointerOffset;
  }
  else if(mode==3){float desired=std::clamp(float(1+(p.x()-initialMouse.x())/std::max(300.,initialRug.width())),.5f,1.8f);float f=initialRug.width()*desired/sim.rw;if(sim.rw*f>300&&sim.rw*f<width()*.96&&sim.rh*f<height()*.96){sim.scale(f);derive();update();}}
  else if(mode==4){sim.rotate(float(delta.x())*.006f);derive();update();}
  e->accept();
 }
 void mouseReleaseEvent(QMouseEvent*e)override{
  if(isLocked){e->ignore();return;}
  bool lifted=mode==2;
  sim.end();mode=0;
  if(lifted){folded=true;wake();}else emit stateSaved();
  unsetCursor();emit interactingChanged();e->accept();
 }
 void mouseDoubleClickEvent(QMouseEvent*e)override{
  if(isLocked){e->ignore();return;}sim.end();mode=0;toggleFold();emit interactingChanged();e->accept();}
 void hoverEnterEvent(QHoverEvent*e)override{if(isLocked){e->ignore();return;}isHovered=true;setCursor(Qt::OpenHandCursor);emit hoveredChanged();QQuickItem::hoverEnterEvent(e);}
 void hoverLeaveEvent(QHoverEvent*e)override{isHovered=false;emit hoveredChanged();QQuickItem::hoverLeaveEvent(e);}
 void wheelEvent(QWheelEvent*e)override{if(isLocked){e->ignore();return;}turn(e->angleDelta().y()>0?3:-3);e->accept();}
 void geometryChange(const QRectF&n,const QRectF&o)override{QQuickItem::geometryChange(n,o);derive();update();}
 QSGNode*updatePaintNode(QSGNode*old,UpdatePaintNodeData*)override{
  auto*root=static_cast<RootNode*>(old);
  if(!root||dirtyImage){delete root;root=new RootNode;dirtyImage=false;if(!image.isNull()){root->texture=window()->createTextureFromImage(image);root->texture->setFiltering(QSGTexture::Linear);root->texture->setMipmapFiltering(QSGTexture::Linear);}
   static QSGGeometry::Attribute attrs[]={QSGGeometry::Attribute::create(0,2,QSGGeometry::FloatType,true),QSGGeometry::Attribute::create(1,2,QSGGeometry::FloatType),QSGGeometry::Attribute::create(2,4,QSGGeometry::FloatType)};
   static QSGGeometry::AttributeSet set={3,sizeof(Vertex),attrs};
   for(int pass=0;pass<2;pass++){auto*n=new QSGGeometryNode;auto*g=new QSGGeometry(set,0);g->setDrawingMode(QSGGeometry::DrawTriangles);g->setVertexDataPattern(QSGGeometry::StreamPattern);n->setGeometry(g);n->setFlag(QSGNode::OwnsGeometry);auto*m=new RugMaterial;m->shadow=pass==0?1:0;m->texture=root->texture;n->setMaterial(m);n->setFlag(QSGNode::OwnsMaterial);root->appendChildNode(n);}
  }
  if(!root->texture)return root;
  std::vector<int> order(sim.faces.size());std::iota(order.begin(),order.end(),0);
  std::stable_sort(order.begin(),order.end(),[&](int a,int b){auto aa=sim.faces[a],bb=sim.faces[b];float az=sim.pos[aa[0]].z+sim.pos[aa[1]].z+sim.pos[aa[2]].z,bz=sim.pos[bb[0]].z+sim.pos[bb[1]].z+sim.pos[bb[2]].z;return az<bz;});
  for(QSGNode*c=root->firstChild();c;c=c->nextSibling()){
   auto*n=static_cast<QSGGeometryNode*>(c);auto*g=n->geometry();g->allocate(int(order.size()*3));auto*out=static_cast<Vertex*>(g->vertexData());
   for(int f:order)for(int k:sim.faces[f]){auto p=projected[k];auto nn=sim.norm[k];*out++={float(p.x()),float(p.y()),float(k%sim.nx)/(sim.nx-1),float(k/sim.nx)/(sim.ny-1),nn.x,nn.y,nn.z,sim.pos[k].z};}
   static_cast<RugMaterial*>(n->material())->size={sim.rw,sim.rh};n->markDirty(QSGNode::DirtyGeometry|QSGNode::DirtyMaterial);
  }
  return root;
 }
};
class DesktopRugPlugin:public QQmlExtensionPlugin{
 Q_OBJECT
 Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:void registerTypes(const char*uri)override{shaderPath=baseUrl().toLocalFile();qmlRegisterType<ClothSurface>(uri,1,0,"ClothSurface");}
};
#include "desktoprug.moc"
