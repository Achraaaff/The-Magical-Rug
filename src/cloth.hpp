// XPBD cloth adapted from siliconbag/under-the-rug (MIT), Cloth.swift.
// Linux implementation: native C++, Qt scene graph rendering, exact pointer movement.
#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <cstdint>
struct V3 {
 float x=0,y=0,z=0;
 V3 operator+(V3 b)const{return {x+b.x,y+b.y,z+b.z};}
 V3 operator-(V3 b)const{return {x-b.x,y-b.y,z-b.z};}
 V3 operator*(float a)const{return {x*a,y*a,z*a};}
 V3& operator+=(V3 b){x+=b.x;y+=b.y;z+=b.z;return *this;}
 V3& operator-=(V3 b){x-=b.x;y-=b.y;z-=b.z;return *this;}
 float len2()const{return x*x+y*y+z*z;}
 float len()const{return std::sqrt(len2());}
};
inline V3 cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
struct Constraint{int a,b;float rest,compliance;};
struct Cloth {
 int nx=38,ny=25,grab=-1; float rw=800,rh=520,spacing=20,thickness=13;
 std::vector<V3> pos,prev,vel,rest,norm;
 std::vector<Constraint> cons;
 std::vector<std::array<int,3>> faces;
 std::vector<float> mass,tethers;
 std::vector<int> grabbed;
 std::vector<V3> offsets;
 std::vector<std::pair<int,int>> pairs;
 V3 grabTarget,grabPosition;
 float maxSpeed=0,rmsSpeed=0;
 int idx(int x,int y)const{return y*nx+x;}
 int count()const{return int(pos.size());}
 void reset(float x,float y,float w,float h) {
  rw=w;rh=h;spacing=w/(nx-1);ny=std::max(8,int(std::round(h/spacing))+1);
  thickness=spacing*1.1f;grab=-1;grabbed.clear();
  int n=nx*ny;pos.resize(n);prev.resize(n);vel.assign(n,{});rest.resize(n);norm.resize(n);mass.assign(n,1);
  cons.clear();faces.clear();
  for(int j=0;j<ny;j++)for(int i=0;i<nx;i++){
   int k=idx(i,j);rest[k]={w*i/(nx-1),h*j/(ny-1),0};pos[k]=rest[k]+V3{x,y,0};prev[k]=pos[k];
  }
  auto con=[&](int a,int b,float c){cons.push_back({a,b,(rest[b]-rest[a]).len(),c});};
  for(int j=0;j<ny;j++)for(int i=0;i<nx;i++){
   int a=idx(i,j);
   if(i+1<nx)con(a,idx(i+1,j),0);
   if(j+1<ny)con(a,idx(i,j+1),0);
   if(i+1<nx&&j+1<ny){con(a,idx(i+1,j+1),2e-7f);con(idx(i+1,j),idx(i,j+1),2e-7f);
    int b=idx(i+1,j),c=idx(i,j+1),d=idx(i+1,j+1);
    if((i+j)%2==0){faces.push_back({a,b,d});faces.push_back({a,d,c});}
    else{faces.push_back({a,b,c});faces.push_back({b,d,c});}
   }
   if(i+2<nx)con(a,idx(i+2,j),3e-3f);
   if(j+2<ny)con(a,idx(i,j+2),3e-3f);
  }
  derive();
 }
 void derive(){
  std::fill(norm.begin(),norm.end(),V3{});
  for(auto f:faces){V3 n=cross(pos[f[1]]-pos[f[0]],pos[f[2]]-pos[f[0]]);for(int k:f)norm[k]+=n;}
  for(auto &n:norm){float l=n.len();n=l>1e-6f?n*(1/l):V3{0,0,1};}
 }
 void translate(float dx,float dy){V3 d{dx,dy,0};for(int k=0;k<count();k++){pos[k]+=d;prev[k]+=d;}}
 V3 center()const{V3 c{};for(auto p:pos)c+=p;return c*(1.f/std::max(1,count()));}
 void rotate(float a){V3 c=center();float ca=std::cos(a),sa=std::sin(a);auto f=[&](V3 p){V3 d=p-c;return V3{c.x+ca*d.x-sa*d.y,c.y+sa*d.x+ca*d.y,p.z};};for(int i=0;i<count();i++){pos[i]=f(pos[i]);prev[i]=f(prev[i]);vel[i]={};}derive();}
 void scale(float factor){V3 c=center();rw*=factor;rh*=factor;spacing*=factor;thickness*=factor;for(int i=0;i<count();i++){pos[i]=c+(pos[i]-c)*factor;prev[i]=pos[i];vel[i]={};rest[i]=rest[i]*factor;}for(auto&v:cons)v.rest*=factor;derive();}
 void begin(int v,V3 target){
  end();grab=v;grabTarget=target;grabPosition=pos[v];
  // A pinch controls position, not the orientation of a rigid patch.
  // Let the corner rotate over the crease instead of forcing it face-up.
  grabbed.push_back(v);offsets.push_back({});mass[v]=0;
  tethers.resize(count());for(int i=0;i<count();i++)tethers[i]=(rest[i]-rest[v]).len()*1.01f;
 }
 float maxStructuralStretch()const{
  float maximum=1;
  for(auto c:cons)if(c.compliance==0&&c.rest>0)maximum=std::max(maximum,(pos[c.b]-pos[c.a]).len()/c.rest);
  return maximum;
 }
 void end(){
  bool wasGrabbing=grab>=0;
  for(int k:grabbed)mass[k]=1;
  if(wasGrabbing)for(auto &v:vel){v=v*.25f;float speed=v.len();if(speed>180)v=v*(180/speed);}
  grabbed.clear();offsets.clear();grab=-1;
 }
 static int64_t hash(int x,int y,int z){return int64_t(x)*73856093 ^ int64_t(y)*19349663 ^ int64_t(z)*83492791;}
 void makePairs(){
  pairs.clear();float cell=thickness*2.4f;std::unordered_map<int64_t,std::vector<int>> h;h.reserve(count()*2);
  for(int i=0;i<count();i++){auto p=pos[i];h[hash(int(std::floor(p.x/cell)),int(std::floor(p.y/cell)),int(std::floor(p.z/cell)))].push_back(i);}
  for(int i=0;i<count();i++){
   auto p=pos[i];int x=int(std::floor(p.x/cell)),y=int(std::floor(p.y/cell)),z=int(std::floor(p.z/cell));
   for(int dz=-1;dz<=1;dz++)for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
    auto it=h.find(hash(x+dx,y+dy,z+dz));if(it==h.end())continue;
    for(int j:it->second)if(j>i&&std::abs(i%nx-j%nx)+std::abs(i/nx-j/nx)>2&&(pos[j]-p).len2()<cell*cell*2.5f)pairs.push_back({i,j});
   }
  }
 }
 void step(float dt){
  const int sub=8;const float sdt=dt/sub, inv=1/(sdt*sdt),g=5000*sdt,damp=std::max(0.f,1-(grab>=0?12.f:10.f)*sdt);
  makePairs();
  for(int substep=0;substep<sub;substep++){
   for(int i=0;i<count();i++){
    prev[i]=pos[i];if(!mass[i])continue;vel[i].z-=g;vel[i]=vel[i]*damp;
    float len=vel[i].len(),cap=.3f*thickness/sdt;if(len>cap)vel[i]=vel[i]*(cap/len);pos[i]+=vel[i]*sdt;
   }
   if(grab>=0){
    // The grabbed patch is under the pointer. Only the rest of the fabric has inertia.
    V3 motion=grabTarget-grabPosition;
    float travel=motion.len(),limit=spacing*.65f;
    grabPosition+=travel>limit?motion*(limit/travel):motion;
    for(size_t n=0;n<grabbed.size();n++)pos[grabbed[n]]=grabPosition+offsets[n];
   }
   // Sweep both ways so errors cannot accumulate from the held corner
   // toward the same edge on every substep.
   for(int sweep=0;sweep<4;sweep++)for(size_t ci=0;ci<cons.size();ci++){
    auto c=cons[(sweep%2)?cons.size()-1-ci:ci];
    float wa=mass[c.a],wb=mass[c.b],w=wa+wb;if(w==0)continue;
    V3 d=pos[c.b]-pos[c.a];float l=d.len();if(l<1e-6f)continue;
    float s=-(l-c.rest)/(w+c.compliance*inv);d=d*(s/l);pos[c.a]-=d*wa;pos[c.b]+=d*wb;
   }
   if(grab>=0)for(int i=0;i<count();i++)if(mass[i]){V3 d=pos[i]-grabPosition;float l=d.len(),m=tethers[i];if(l>m&&l>0)pos[i]=grabPosition+d*(m/l);}
   for(int pass=0;pass<2;pass++)for(auto [i,j]:pairs){
    float wi=mass[i],wj=mass[j],w=wi+wj;if(!w)continue;V3 d=pos[j]-pos[i];float d2=d.len2(),t2=thickness*thickness;
    if(d2>=t2||d2<1e-10f)continue;float l=std::sqrt(d2);d=d*((thickness-l)/(l*w));pos[i]-=d*wi;pos[j]+=d*wj;
    // Contact friction acts along the surface. Damping the collision
    // normal here would pull the separated layers back through each other.
    V3 normal=(pos[j]-pos[i]);normal=normal*(1/std::max(normal.len(),1e-6f));
    V3 relative=(pos[j]-prev[j])-(pos[i]-prev[i]);
    V3 tangent=relative-normal*(relative.x*normal.x+relative.y*normal.y+relative.z*normal.z);
    float slide=tangent.len(),penetration=thickness-l;
    float friction=slide<2.f*penetration?1.f:std::min(1.f,1.3f*penetration/std::max(slide,1e-6f));
    pos[i]+=tangent*(friction*wi/w);pos[j]-=tangent*(friction*wj/w);
   }
   for(int i=0;i<count();i++){
    if(mass[i]&&pos[i].z<0){float pen=-pos[i].z;pos[i].z=0;float dx=pos[i].x-prev[i].x,dy=pos[i].y-prev[i].y,l=std::sqrt(dx*dx+dy*dy);
     if(l<2*pen){pos[i].x=prev[i].x;pos[i].y=prev[i].y;}else if(l>0){float k=std::min(1.3f*pen/l,1.f);pos[i].x-=dx*k;pos[i].y-=dy*k;}
    }
    vel[i]=(pos[i]-prev[i])*(1/sdt);
   }
  }
  maxSpeed=0;float total=0;for(int i=0;i<count();i++)if(mass[i]){float v=vel[i].len2();maxSpeed=std::max(maxSpeed,v);total+=v;}
  maxSpeed=std::sqrt(maxSpeed);rmsSpeed=std::sqrt(total/std::max(1,count()));derive();
 }
 void foldCorner(){
  end();float cut=std::min(rw,rh)*.74f,radius=std::max(5.f,spacing*.32f),pi=3.14159265f;
  V3 corner=pos[nx-1];
  for(int i=0;i<count();i++){
   float d=((pos[i].x-corner.x)-(pos[i].y-corner.y)+cut)/1.41421356f;
   if(d>0){float theta=std::min(d/radius,pi),nd=d<pi*radius?radius*std::sin(theta):-(d-pi*radius);
    pos[i].x+=(nd-d)*.70710678f;pos[i].y-=(nd-d)*.70710678f;pos[i].z=radius*(1-std::cos(theta))+1;
   }
   prev[i]=pos[i];vel[i]={};
  }
  derive();
 }
};
