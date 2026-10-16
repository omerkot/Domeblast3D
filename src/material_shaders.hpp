#pragma once
namespace db {
constexpr const char *materialVS = R"GLSL(
attribute vec3 pos;attribute vec4 color;attribute vec2 uv;
attribute vec3 normal;attribute float material;
uniform mat4 m;
varying vec4 c;varying vec2 t;varying vec3 wp;varying vec3 n;varying float mat;
void main(){gl_Position=m*vec4(pos,1.);c=color;t=uv;wp=pos;n=normal;mat=material;}
)GLSL";
constexpr const char *materialFS = R"GLSL(
#if defined(DB_MANAGED_COLOR) && defined(GL_FRAGMENT_PRECISION_HIGH)
precision highp float;
#else
precision mediump float;
#endif
varying vec4 c;varying vec2 t;varying vec3 wp;varying vec3 n;varying float mat;
uniform sampler2D tex;uniform sampler2D atlas;
uniform float textMode;uniform float targetGlow;uniform vec3 eye;uniform vec3 fogColor;
uniform vec4 impact;uniform float linearScene;
vec3 linearColor(vec3 v){
#ifdef DB_MANAGED_COLOR
 v=max(v,vec3(0.));return mix(v/12.92,pow((v+.055)/1.055,vec3(2.4)),step(vec3(.04045),v));
#else
 return v;
#endif
}
vec3 displayColor(vec3 v){
 v=max(v,vec3(0.));return mix(v*12.92,1.055*pow(v,vec3(1./2.4))-.055,step(vec3(.0031308),v));
}
vec3 storeLit(vec3 light){
#ifdef DB_MANAGED_COLOR
 light=max(light,vec3(0.));if(linearScene>.5)return light;
 vec3 exposed=light*1.6;return displayColor(exposed/(1.+max(exposed.r,max(exposed.g,exposed.b))));
#else
 return sqrt(max(vec3(0.),vec3(1.)-exp(-light*1.25)));
#endif
}
vec3 storeUnlit(vec3 color){
#ifdef DB_MANAGED_COLOR
 if(linearScene>.5)return linearColor(color);
#endif
 return color;
}
vec2 tileUV(vec2 uv,vec2 cell){return cell+vec2(.003)+fract(uv)*.494;}
void main(){
 if(mat<-.5){vec2 q=t*2.-1.;float rr=dot(q,q);float a=pow(max(0.,1.-rr),mat< -1.5?1.1:2.8);gl_FragColor=vec4(storeUnlit(c.rgb),c.a*a);return;}
 if(mat<.5){vec4 q=texture2D(tex,t);gl_FragColor=c*mix(q,vec4(1.,1.,1.,q.r),textMode);gl_FragColor.rgb=storeUnlit(mix(gl_FragColor.rgb,vec3(1.,.42,.035),clamp(targetGlow*.65,0.,.75)));return;}
 vec3 N=normalize(n);vec3 V=normalize(eye-wp);vec3 L=normalize(vec3(-.45,.85,.35));
 if(mat>9.5){
   // Matte sand: filtered fine grain and low-amplitude wind ripples.
   float grain=dot(texture2D(atlas,tileUV(wp.xz*1.25,vec2(0.,.5))).rgb,vec3(.333));
   float phase=dot(wp.xz,vec2(.92,.28))*24.+1.5*sin(wp.z*.7);
   N=normalize(N-vec3(.92,0.,.28)*cos(phase)*.035);
   float variation=.86+.28*grain+.035*sin(phase);
   vec3 sand=linearColor(vec3(.64,.45,.25))*variation*linearColor(c.rgb);
   float shelter=.82+.18*smoothstep(.08,1.25,wp.y);
   vec3 lit=sand*(.30+.82*max(dot(N,L),0.))*shelter;
   lit+=vec3(.035,.045,.055)*(1.-max(dot(N,V),0.));
   vec3 dp=impact.xyz-wp;lit+=vec3(1.,.24,.035)*impact.w/(1.+dot(dp,dp)*.24)*(sand*.7+.2);
   gl_FragColor=vec4(storeLit(lit),c.a);return;
 }

 bool readableRock=mat>6.5&&mat<8.5;bool paintedHull=mat>8.5&&mat<9.5;
 vec2 cell=mat<1.5?vec2(0.):mat<2.5?vec2(.5,0.):mat<3.5?vec2(0.,.5):mat<4.5?vec2(.5,.5):vec2(.5,0.);
 if(readableRock)cell=mat<7.5?vec2(0.,.5):vec2(.5,.5);
 vec2 uv=tileUV(t,cell);vec3 encodedAlbedo=texture2D(atlas,uv).rgb;vec3 albedo=linearColor(encodedAlbedo);
 float hx=dot(texture2D(atlas,tileUV(t+vec2(.003,0.),cell)).rgb-encodedAlbedo,vec3(.333));
 float hy=dot(texture2D(atlas,tileUV(t+vec2(0.,.003),cell)).rgb-encodedAlbedo,vec3(.333));
 vec3 T=normalize(abs(N.y)>.85?cross(N,vec3(0.,0.,1.)):cross(N,vec3(0.,1.,0.)));
 vec3 B=cross(N,T);
 N=normalize(N-(T*hx+B*hy)*((mat>2.5&&mat<4.5)||readableRock?2.5:1.2));
 float metal=mat<2.5?.62:.08;
 float rough=mat<1.5?.42:mat<2.5?.24:.75;
 if(mat>5.5&&mat<6.5){albedo=linearColor(vec3(.16,.35,.45));metal=.9;rough=.10;}
 float diff=max(dot(N,L),0.);
 float spec=pow(max(dot(N,normalize(L+V)),0.),mix(80.,8.,rough))*(mat>2.5&&mat<4.5?.035:.3+metal*.8);
 float fres=pow(1.-max(dot(N,V),0.),3.);
 vec3 base=albedo*linearColor(c.rgb);
 vec3 lit=base*(.28+.75*diff)+vec3(.63,.78,.96)*spec+vec3(.12,.25,.40)*fres*(mat>2.5&&mat<4.5?.07:.4+metal);
 if(mat>3.5&&mat<4.5)lit+=max(albedo.r-albedo.b*1.4,0.)*vec3(2.2,.4,.04);
 if(readableRock){
   // Preserve stone texture and deep fissures; a raised diffuse floor keeps
   // the irregular silhouette readable without toon bands or a drawn outline.
   float grain=clamp(dot(encodedAlbedo,vec3(.333)),0.,1.);
   float cavity=.55+.45*smoothstep(.05,.34,grain);
   vec3 stone=linearColor(c.rgb)*(.30+.75*sqrt(grain))*cavity;
   float back=max(dot(N,normalize(vec3(.5,.25,-.8))),0.);
   lit=stone*(.38+.86*diff);
   lit+=vec3(.17,.20,.24)*back*.30;
   lit+=vec3(.13,.15,.17)*fres;
   lit+=vec3(.025)*pow(max(dot(N,normalize(L+V)),0.),18.);
   if(mat>7.5)lit+=max(albedo.r-albedo.b*1.4,0.)*vec3(1.05,.20,.035);
 }
 if(paintedHull){
   // Full panel texture and restrained metal highlights, including dark seams.
   vec3 paint=linearColor(c.rgb)*(.30+.70*albedo);
   float finish=pow(max(dot(N,normalize(L+V)),0.),36.);
   lit=paint*(.43+.68*diff)+vec3(.12,.14,.16)*finish;
   lit+=vec3(.10,.12,.14)*fres;
 }
 // Concentrate the lock highlight on the real silhouette. Preserve surface
 // colors and texture, and leave existing sprite halos/trails at normal intensity.
 float lockRim=pow(1.-abs(dot(N,V)),2.);
 lit+=targetGlow*vec3(3.,.35,.008)*(.035+1.8*lockRim);
 vec3 dp=impact.xyz-wp;float atten=impact.w/(1.+dot(dp,dp)*.24);
 lit+=vec3(1.,.24,.035)*atten*(base*.7+.2);
 float fog=clamp((length(eye-wp)-25.)/40.,0.,.6);
 if(readableRock||paintedHull)fog*=.10;
 lit=mix(lit,linearColor(fogColor),fog);
 gl_FragColor=vec4(storeLit(lit),c.a);
}
)GLSL";
// Only the target's actual opaque geometry or star points contribute to the mask.
constexpr const char *targetMaskFS = R"GLSL(
precision mediump float;varying vec2 t;varying float mat;
void main(){
 if(mat<-.5&&length(t*2.-1.)>1.)discard;
 gl_FragColor=vec4(1.);
}
)GLSL";
constexpr const char *targetHaloFS = R"GLSL(
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
varying vec2 t;
uniform sampler2D maskTex;uniform vec2 pixelSize;uniform float strength;uniform float radiusScale;uniform float vrHalo;
void main(){
 float inside=texture2D(maskTex,t).r;
 if(inside>.99)discard;
 if(vrHalo>.5){
   // Dense Gaussian coverage gives VR a narrow, smooth contour instead of
   // scaling up the handheld's sparse maximum-filter samples. No dithering.
   float coverage=0.;float total=0.;
   for(int y=-3;y<=3;y++)for(int x=-3;x<=3;x++){
     vec2 offset=vec2(float(x),float(y));
     float weight=exp(-dot(offset,offset)*.55);
     coverage+=texture2D(maskTex,t+offset*pixelSize*radiusScale).r*weight;
     total+=weight;
   }
   // Keep the smooth narrow footprint, but make the orange target edge readable
   // at tabletop distance. This shader does not draw the bomb's round glow.
   float alpha=min(.82,sqrt(max(coverage/total,0.))*(1.1+.1*strength))*(1.-inside);
   gl_FragColor=vec4(1.,.42,.035,alpha);return;
 }
 float aura=0.;
 // Retain dense sampling as the contour halo grows to twice its former width.
 for(int y=-6;y<=6;y++)for(int x=-6;x<=6;x++){
   vec2 offset=vec2(float(x),float(y))*.5;
   float weight=max(0.,1.-length(offset)/3.6);
   aura=max(aura,texture2D(maskTex,t+offset*pixelSize*radiusScale).r*weight);
 }
 gl_FragColor=vec4(1.,.42,.035,min(.74,.5+strength*.23)*aura*(1.-inside));
}
)GLSL";
constexpr const char *finishFS = R"GLSL(
#if defined(DB_MANAGED_COLOR) && defined(GL_FRAGMENT_PRECISION_HIGH)
precision highp float;
#else
precision mediump float;
#endif
varying vec2 t;uniform sampler2D tex;uniform vec2 pixelSize;uniform float vignetteStrength;uniform float linearScene;
vec3 encodeSRGB(vec3 v){v=max(v,vec3(0.));return mix(v*12.92,1.055*pow(v,vec3(1./2.4))-.055,step(vec3(.0031308),v));}
void main(){
 vec2 d=pixelSize;vec3 c=texture2D(tex,t).rgb;
#ifdef DB_MANAGED_COLOR
 // Linear-light compositing and one display conversion; no softening pass.
 if(linearScene>.5){
   vec3 glow=vec3(0.);
   for(int i=-2;i<=2;i++){vec2 q=float(i)*d*2.5;glow+=max(texture2D(tex,t+q).rgb-1.,0.);glow+=max(texture2D(tex,t+vec2(q.x,-q.y)).rgb-1.,0.);}
   vec3 exposed=max(c+glow*.025,vec3(0.))*1.6;
   c=encodeSRGB(exposed/(1.+max(exposed.r,max(exposed.g,exposed.b))));
 }
 // Sub-LSB display dithering reduces coherent bands without visible grain.
 float noise=fract(52.9829189*fract(dot(gl_FragCoord.xy,vec2(.06711056,.00583715))))-.5;
 gl_FragColor=vec4(clamp(c+noise/255.,0.,1.),1.);return;
#else
 vec3 l=texture2D(tex,t-vec2(d.x,0.)).rgb,r=texture2D(tex,t+vec2(d.x,0.)).rgb;
 vec3 u=texture2D(tex,t-vec2(0.,d.y)).rgb,b=texture2D(tex,t+vec2(0.,d.y)).rgb;
 float edge=max(length(l-r),length(u-b));c=mix(c,(l+r+u+b)*.25,clamp(edge*.5,0.,.32));
 vec3 glow=vec3(0.);for(int i=-2;i<=2;i++){vec2 q=float(i)*d*2.5;glow+=max(texture2D(tex,t+q).rgb-.73,0.);glow+=max(texture2D(tex,t+vec2(q.x,-q.y)).rgb-.73,0.);}
 float vignette=1.-vignetteStrength*.17*dot((t-.5)*1.4,(t-.5)*1.4);
 gl_FragColor=vec4((c+glow*.14)*vignette,1.);
#endif
}
)GLSL";
} // namespace db
