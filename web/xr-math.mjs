import {stick} from './input.mjs';

export function multiply(a,b) {
  const out=new Float32Array(16);
  for(let c=0;c<4;c++)for(let r=0;r<4;r++)
    for(let k=0;k<4;k++)out[c*4+r]+=a[k*4+r]*b[c*4+k];
  return out;
}
export function tableTransform(viewer,scale=.08) {
  const yaw=Math.atan2(viewer[8],viewer[10]),c=Math.cos(yaw),s=Math.sin(yaw);
  // Anchor once, not every frame: looking or leaning never drags the table along.
  return new Float32Array([c*scale,0,-s*scale,0, 0,scale,0,0, s*scale,0,c*scale,0,
    viewer[12]-s*2.55,viewer[13]-.55,viewer[14]-c*2.55,1]);
}
export function inverseTable(m) {
  const out=new Float32Array(16),scale2=m[0]**2+m[1]**2+m[2]**2;
  for(let c=0;c<3;c++)for(let r=0;r<3;r++)out[c*4+r]=m[r*4+c]/scale2;
  for(let r=0;r<3;r++)out[12+r]=-(out[r]*m[12]+out[4+r]*m[13]+out[8+r]*m[14]);
  out[15]=1;return out;
}
export function rotateTable(anchor,angle) {
  const c=Math.cos(angle),s=Math.sin(angle);
  // Rotate about the table's own centre, preserving its physical location/scale.
  return multiply(anchor,new Float32Array([c,0,-s,0,0,1,0,0,s,0,c,0,0,0,0,1]));
}
export function gripPressure(pad) {
  const button=pad?.buttons[1],value=button?.value;
  const raw=Number.isFinite(value)?Math.min(1,Math.max(0,value)):(value==null&&button?.pressed?1:0);
  // Ignore resting sensor noise; retain continuous pressure beyond the deadzone.
  return raw<=.08?0:(raw-.08)/.92;
}
export function matricesForView(projection,eye,model,inverseModel) {
  const result=new Float32Array(32);
  result.set(multiply(projection,multiply(eye.inverse.matrix,model)));
  result.set(multiply(inverseModel,eye.matrix),16);
  return result;
}
export function readXRInput(sources) {
  const usable=Array.from(sources).filter(s=>s.gamepad?.mapping==='xr-standard'&&s.gamepad.connected!==false);
  const left=usable.find(s=>s.handedness==='left')?.gamepad;
  const right=usable.find(s=>s.handedness==='right')?.gamepad;
  const pressed=(pad,index)=>!!pad?.buttons[index]?.pressed||(pad?.buttons[index]?.value??0)>.5;
  const [mx,my]=stick(left?.axes[2],left?.axes[3]);
  const [ax,ay]=stick(right?.axes[2],right?.axes[3]);
  return {mx,my,ax,ay,orbit:gripPressure(right)-gripPressure(left),fire:pressed(right,0),focus:pressed(left,3),both:!!left&&!!right,
    // A / B / X / Y and right trigger. System buttons are deliberately not bound.
    buttons:[pressed(right,4),pressed(right,5),pressed(left,4),pressed(left,5),pressed(right,0)]};
}
