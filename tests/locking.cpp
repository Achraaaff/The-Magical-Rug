#include <QtGui/QGuiApplication>
#include <QtGui/QHoverEvent>
#include <QtGui/QWheelEvent>
#include <QtQuick/QQuickWindow>
#include <QtQml/QQmlApplicationEngine>
#include <QtTest/QTest>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <iostream>
#include <cstdlib>
void check(bool good,const char*why){if(!good){std::cerr<<"FAIL: "<<why<<std::endl;std::exit(1);}}
QJsonObject state(QObject*o){return QJsonDocument::fromJson(o->property("stateJson").toString().toUtf8()).object();}
QJsonObject diag(QObject*o){QString s;QMetaObject::invokeMethod(o,"diagnostics",Q_RETURN_ARG(QString,s));return QJsonDocument::fromJson(s.toUtf8()).object();}
bool hit(QObject*o,float x,float y){bool v=false;QMetaObject::invokeMethod(o,"hit",Q_RETURN_ARG(bool,v),Q_ARG(qreal,x),Q_ARG(qreal,y));return v;}
void toggle(QObject*o){check(QMetaObject::invokeMethod(o,"ToggleLock"),"toggle unavailable");QCoreApplication::processEvents();}
int main(int argc,char**argv){
 QGuiApplication app(argc,argv);QQmlApplicationEngine engine;engine.load(QUrl::fromLocalFile(argv[1]));check(!engine.rootObjects().isEmpty(),"QML load failed");auto*w=qobject_cast<QQuickWindow*>(engine.rootObjects()[0]);auto*c=w->findChild<QObject*>("cloth");check(c,"missing cloth");QTest::qWait(100);
 check(!c->property("locked").toBool()&&hit(c,525,370),"initial rug isn't interactive");
 // Lock during an actual drag, so the current pose and mouse grab must both freeze.
 QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,{525,370});QTest::mouseMove(w,{565,390});check(diag(c)["mode"].toInt()==1,"unlocked body drag failed");toggle(c);
 auto saved=c->property("stateJson").toString();QJsonValue pose=state(c)["positions"];
 check(c->property("locked").toBool()&&!c->property("enabled").toBool(),"lock did not disable item");check(!hit(c,565,390)&&!c->property("hovered").toBool(),"lock still accepts hover/hit");check(diag(c)["mode"].toInt()==0&&c->property("sleeping").toBool(),"lock did not stop active drag");
 QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,{565,390});QTest::mouseMove(w,{700,400});QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,{565,390});QTest::mouseMove(w,{700,450});QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,{700,450});QTest::mouseDClick(w,Qt::LeftButton,Qt::NoModifier,{565,390});
 QHoverEvent hover(QEvent::HoverEnter,{565,390},{565,390},{565,390});QCoreApplication::sendEvent(c,&hover);
 QWheelEvent wheel({565,390},{565,390},{0,0},{0,120},Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QCoreApplication::sendEvent(c,&wheel);
 QMetaObject::invokeMethod(c,"toggleFold");QMetaObject::invokeMethod(c,"turn",Q_ARG(float,10));QMetaObject::invokeMethod(c,"resizeRug",Q_ARG(float,1.1f));QTest::qWait(150);
 std::cout<<"Lock after input: positions="<<(state(c)["positions"]==pose)<<", hover="<<c->property("hovered").toBool()<<", state="<<QJsonDocument(diag(c)).toJson(QJsonDocument::Compact).constData()<<std::endl;check(state(c)["positions"]==pose&&!c->property("hovered").toBool(),"locked input changed mesh or hover");
 toggle(c);check(hit(c,565,390)&&c->property("enabled").toBool(),"unlock did not restore input");check(state(c)["positions"]==pose,"unlock moved rug");
 c->setProperty("stateJson",saved);check(c->property("locked").toBool()&&!hit(c,565,390),"saved lock did not restore");check(state(c)["positions"]==pose,"restore changed pose");toggle(c);
 QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,{565,390});QTest::mouseMove(w,{615,420});QTest::mouseRelease(w,Qt::LeftButton,Qt::NoModifier,{615,420});check(state(c)["positions"]!=pose,"restored unlock failed to move");
 auto b=c->property("clothBounds").toRectF();QPoint corner(int(b.right()-10),int(b.top()+10));QTest::mousePress(w,Qt::LeftButton,Qt::NoModifier,corner);check(diag(c)["mode"].toInt()==2,"unlock did not restore corner physics");QTest::mouseMove(w,corner+QPoint(-60,40));QTest::qWait(100);toggle(c);check(c->property("sleeping").toBool()&&diag(c)["mode"].toInt()==0,"lock during live fold failed");QJsonValue frozen=state(c)["positions"];QTest::qWait(150);check(state(c)["positions"]==frozen,"locked fold continued moving");
 std::cout<<"PASS: lock during drag/fold, no hover/click/wheel/movement, click-through, saved-state restore, unlock restores body and corner dragging."<<std::endl;
}
