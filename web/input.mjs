import {normalizeProfile,readBinding,readMappedAxis} from './controller-mapping.mjs';
export const VR_QUALITY = Object.freeze({
  performance: Object.freeze({label:'Performance',scale:.75,maxEdge:1280,foveation:.5,hudScale:1}),
  balanced: Object.freeze({label:'Balanced',scale:1,maxEdge:1920,foveation:.15,hudScale:1.5}),
  high: Object.freeze({label:'High',scale:1,maxEdge:2048,foveation:0,hudScale:1.5}),
});
export function stick(x = 0, y = 0, deadzone = .16) {
  if (!Number.isFinite(x) || !Number.isFinite(y)) return [0,0];
  const magnitude = Math.hypot(x,y);
  if (magnitude <= deadzone) return [0,0];
  const gain = Math.min(1,(magnitude-deadzone)/(1-deadzone))/magnitude;
  return [x*gain,y*gain];
}
export function readPad(pad,profile=null) {
  if (!pad?.connected || pad.mapping === 'xr-standard') return null;
  if(profile){
    const mapping=normalizeProfile(profile,pad);if(!mapping)return null;
    const [mx,my]=stick(readMappedAxis(pad,mapping.axes.moveX),readMappedAxis(pad,mapping.axes.moveY),mapping.deadzone);
    const [ax,ay]=stick(readMappedAxis(pad,mapping.axes.aimX),readMappedAxis(pad,mapping.axes.aimY),mapping.deadzone);
    const buttons=mapping.buttons.map(b=>readBinding(pad,b));
    // Controllers without a separate D-pad can navigate menus with the left stick.
    for(const [index,value]of [[12,my<-.6],[13,my>.6],[14,mx<-.6],[15,mx>.6]])
      if(!mapping.buttons[index])buttons[index]=value;
    return {mx,my,ax,ay,fire:buttons[7],focus:buttons[10],orbit:Number(buttons[5])-Number(buttons[4]),buttons};
  }
  if(pad.mapping!=='standard')return null;
  const pressed = i => !!pad.buttons[i]?.pressed || (pad.buttons[i]?.value ?? 0) > .5;
  const [mx,my] = stick(pad.axes[0],pad.axes[1]);
  const [ax,ay] = stick(pad.axes[2],pad.axes[3]);
  return {mx,my,ax,ay,fire:pressed(7),focus:pressed(10),orbit:Number(pressed(5))-Number(pressed(4)),
    buttons:Array.from({length:17},(_,i)=>pressed(i))};
}
export function edges(current,previous=[]) {return current.map((v,i)=>v&&!previous[i]);}
export function sanitizeSave(input={}) {
  if(!input||typeof input!=='object')input={};
  const n=(key,fallback)=>Number.isFinite(input[key])?Math.round(Math.max(0,Math.min(100,input[key]))):fallback;
  return {high:Number.isSafeInteger(input.high)?Math.max(0,Math.min(2147483647,input.high)):0,
    autoFire:typeof input.autoFire==='boolean'?input.autoFire:true,classic:input.classic===true,
    master:n('master',65),music:n('music',80),effects:n('effects',100),muted:input.muted===true,
    vrQuality:Object.hasOwn(VR_QUALITY,input.vrQuality)?input.vrQuality:'high',
    vrColors:input.vrColors==='original'?'original':'enhanced'};
}
