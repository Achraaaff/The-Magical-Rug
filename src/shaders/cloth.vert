#version 440
layout(location=0) in vec2 position;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 normalHeight;
layout(location=0) out vec2 texCoord;
layout(location=1) out vec4 normalZ;
layout(std140,binding=0) uniform buf { mat4 qt_Matrix; float qt_Opacity; float shadowPass; vec2 rugSize; };
void main(){
 vec2 p=position;
 if(shadowPass>0.5)p+=vec2(normalHeight.w*.28+2.,normalHeight.w*.40+5.);
 texCoord=uv;normalZ=normalHeight;gl_Position=qt_Matrix*vec4(p,0.,1.);
}
