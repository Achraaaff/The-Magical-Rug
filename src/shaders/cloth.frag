#version 440
layout(location=0) in vec2 texCoord;
layout(location=1) in vec4 normalZ;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform buf { mat4 qt_Matrix; float qt_Opacity; float shadowPass; vec2 rugSize; };
layout(binding=1) uniform sampler2D source;
void main(){
 vec2 edge2=min(texCoord,1.-texCoord)*rugSize;
 float edge=min(edge2.x,edge2.y);
 if(shadowPass>0.5){float a=.24*smoothstep(0.,7.,edge)*qt_Opacity;fragColor=vec4(.025*a,.018*a,.012*a,a);return;}
 vec3 col=texture(source,texCoord).rgb;
 vec3 N=normalize(normalZ.xyz);bool back=N.z<0.;if(back)N=-N;
 float luma=dot(col,vec3(.299,.587,.114));
 if(back){col=mix(col,vec3(luma),.46)*.72+vec3(.06,.045,.025);vec2 knot=fract(texCoord*vec2(330.,220.));float weave=smoothstep(0.,.17,min(min(knot.x,1.-knot.x),min(knot.y,1.-knot.y)));col*=mix(.87,1.,weave);}
 vec2 bump=vec2(dFdx(luma),dFdy(luma))*(back?.35:1.1);N=normalize(N-vec3(bump,0.));
 vec3 L=normalize(vec3(-.3,-.45,1.));float shade=(.32+.9*max(dot(N,L),0.))/(.32+.9*L.z);
 col*=mix(.64,1.,smoothstep(.3,3.,edge));
 // Opaque, material-attached UVs on both sides; no copy of the flat rug beneath a transparent flap.
 fragColor=vec4(min(col*min(shade,1.25),vec3(1.)),1.)*qt_Opacity;
}
