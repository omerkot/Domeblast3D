const clamp=(v,lo,hi)=>Math.max(lo,Math.min(hi,v));
export const PROFILE_KEY='domeblast.controllers.v1';
export const AXIS_NAMES=['moveX','moveY','aimX','aimY'];
export const SETUP_STEPS=[
  {axis:'moveX',prompt:'Move the LEFT stick fully RIGHT',required:true},
  {axis:'moveY',prompt:'Move the LEFT stick fully DOWN',required:true},
  {axis:'aimX',prompt:'Move the RIGHT stick fully RIGHT'},
  {axis:'aimY',prompt:'Move the RIGHT stick fully DOWN'},
  {button:0,prompt:'Press your LAUNCH / CONFIRM button',required:true},
  {button:1,prompt:'Press your BACK button',required:true},
  {button:9,prompt:'Press START / PAUSE',required:true},
  {button:7,prompt:'Pull your FIRE trigger fully',required:true},
  {button:4,prompt:'Press the LEFT shoulder for camera rotation',required:true},
  {button:5,prompt:'Press the RIGHT shoulder for camera rotation',required:true},
  {button:3,prompt:'Press your AUTOFIRE toggle button'},
  {button:2,prompt:'Press your MUSIC toggle button'},
  {button:10,prompt:'Click the LEFT stick for precision movement'},
  {button:11,prompt:'Click the RIGHT stick for fire cadence'},
  {button:12,prompt:'Press D-PAD UP',dpad:true},
  {button:13,prompt:'Press D-PAD DOWN',dpad:true},
  {button:14,prompt:'Press D-PAD LEFT',dpad:true},
  {button:15,prompt:'Press D-PAD RIGHT',dpad:true},
];
export function controllerKey(pad){return JSON.stringify([String(pad.id||'Controller'),pad.mapping||'',pad.axes.length,pad.buttons.length]);}
export function snapshot(pad){return {axes:Array.from(pad.axes,v=>Number.isFinite(v)?v:0),buttons:Array.from(pad.buttons,b=>b?.pressed?1:Number.isFinite(b?.value)?clamp(b.value,0,1):0)};}
export function normalizeProfile(raw,pad){
  if(!raw||raw.version!==1||!raw.axes||!Array.isArray(raw.buttons)||raw.buttons.length!==17)return null;
  const index=(n,count)=>Number.isInteger(n)&&n>=0&&n<count&&n<128;
  const finite=v=>Number.isFinite(v)&&Math.abs(v)<=4;
  const axes={};
  for(const name of AXIS_NAMES){
    const b=raw.axes[name];if(b==null){axes[name]=null;continue;}
    if(!index(b.index,pad.axes.length)||![1,-1].includes(b.sign)||!Number.isFinite(b.center)||Math.abs(b.center)>.5)return null;
    axes[name]={index:b.index,sign:b.sign,center:b.center};
  }
  if(!axes.moveX||!axes.moveY)return null;
  const used=AXIS_NAMES.filter(k=>axes[k]).map(k=>axes[k].index);if(new Set(used).size!==used.length)return null;
  const buttons=[];
  for(const b of raw.buttons){
    if(b==null){buttons.push(null);continue;}
    if(b.kind==='button'&&index(b.index,pad.buttons.length)){buttons.push({kind:'button',index:b.index});continue;}
    if(['axis','target'].includes(b.kind)&&index(b.index,pad.axes.length)&&finite(b.rest)&&finite(b.target)&&Math.abs(b.rest-b.target)>.1){
      const result={kind:b.kind,index:b.index,rest:b.rest,target:b.target};
      if(b.kind==='target'){
        result.tolerance=Number.isFinite(b.tolerance)?clamp(b.tolerance,.02,.35):.06;
        result.period=Number.isFinite(b.period)&&b.period>0&&b.period<=4?b.period:0;
      }
      buttons.push(result);continue;
    }
    return null;
  }
  if([0,1,4,5,7,9].some(i=>!buttons[i]))return null;
  return {version:1,axes,buttons,deadzone:Number.isFinite(raw.deadzone)?clamp(raw.deadzone,.05,.35):.16};
}
export function readBinding(pad,b){
  if(!b)return false;
  if(b.kind==='button')return !!pad.buttons[b.index]?.pressed||(pad.buttons[b.index]?.value||0)>.5;
  const value=pad.axes[b.index];if(!Number.isFinite(value))return false;
  if(b.kind==='axis')return (value-b.rest)/(b.target-b.rest)>.5;
  if(Math.abs(value-b.rest)<.08)return false;
  let distance=Math.abs(value-b.target);
  if(b.period){if(value < -1.05 || value > 1.05)return false;distance=Math.min(distance,Math.abs(b.period-distance));}
  return distance<=b.tolerance;
}
export function readMappedAxis(pad,b){
  if(!b)return 0;const raw=pad.axes[b.index];if(!Number.isFinite(raw))return 0;
  const delta=(raw-b.center)*b.sign;
  const extent=delta>=0?1-b.center*b.sign:1+b.center*b.sign;
  return clamp(delta/Math.max(.1,extent),-1,1);
}
export class ControllerProfiles {
  constructor(storage){this.storage=storage;this.profiles=Object.create(null);try{const data=JSON.parse(storage?.getItem(PROFILE_KEY)||'{}');if(data&&typeof data==='object'&&!Array.isArray(data))for(const [key,value]of Object.entries(data).slice(-32))this.profiles[key]=value;}catch{}}
  get(pad){return pad?normalizeProfile(this.profiles[controllerKey(pad)],pad):null;}
  save(pad,profile){const clean=normalizeProfile(profile,pad);if(!clean)throw Error('Complete the required controls first.');this.profiles[controllerKey(pad)]=clean;return this.persist();}
  remove(pad){delete this.profiles[controllerKey(pad)];return this.persist();}
  persist(){try{const entries=Object.entries(this.profiles).slice(-32);this.profiles=Object.assign(Object.create(null),Object.fromEntries(entries));this.storage?.setItem(PROFILE_KEY,JSON.stringify(this.profiles));return !!this.storage;}catch{return false;}}
}
export class ControllerCalibration {
  constructor(pad){this.key=controllerKey(pad);this.rest=snapshot(pad);this.index=0;this.phase='release';this.neutralSince=null;this.notice='';this.profile={version:1,axes:{},buttons:Array(17).fill(null),deadzone:.16};}
  get step(){return SETUP_STEPS[this.index];}
  get done(){return this.index>=SETUP_STEPS.length;}
  get canSave(){return SETUP_STEPS.filter(s=>s.required).every(s=>s.axis?this.profile.axes[s.axis]:this.profile.buttons[s.button]);}
  next(){this.index++;this.phase='release';this.neutralSince=null;this.notice='';}
  skip(){if(this.step&&!this.step.required)this.next();}
  back(){
    this.index=Math.max(0,this.index-1);
    for(const s of SETUP_STEPS.slice(this.index)){if(s.axis)delete this.profile.axes[s.axis];else this.profile.buttons[s.button]=null;}
    this.phase='release';this.neutralSince=null;this.notice='';
  }
  poll(pad,time){
    if(!pad||controllerKey(pad)!==this.key){this.phase='release';this.neutralSince=null;this.notice='Reconnect the same controller to continue.';return;}
    if(this.done)return;
    const current=snapshot(pad);
    if(this.phase==='release'){
      const neutral=current.buttons.every(v=>v<.2)&&current.axes.every((v,i)=>Math.abs(v-this.rest.axes[i])<.18);
      if(!neutral){this.neutralSince=null;return;}
      if(this.neutralSince===null)this.neutralSince=time;
      if(time-this.neutralSince>=250){this.phase='capture';this.rest.buttons=current.buttons.slice();this.notice='';}
      return;
    }
    const s=this.step,usedAxes=new Set(Object.values(this.profile.axes).filter(Boolean).map(a=>a.index));
    const candidates=current.axes.map((value,index)=>({index,value,delta:value-this.rest.axes[index]}))
      .filter(a=>!usedAxes.has(a.index)).sort((a,b)=>Math.abs(b.delta)-Math.abs(a.delta));
    if(s.axis){
      const axis=candidates.find(a=>Math.abs(a.delta)>.55&&Math.abs(this.rest.axes[a.index])<=.5&&Math.abs(a.value)<=1.05);
      if(axis){this.profile.axes[s.axis]={index:axis.index,sign:Math.sign(axis.delta),center:this.rest.axes[axis.index]};this.next();}
      return;
    }
    const button=current.buttons.findIndex((value,index)=>value>.65&&this.rest.buttons[index]<.2);
    let binding=button>=0?{kind:'button',index:button}:null;
    if(!binding){const axis=candidates.find(a=>Math.abs(a.delta)>(s.dpad ? .12 : .55));if(axis)binding={kind:s.dpad?'target':'axis',index:axis.index,rest:this.rest.axes[axis.index],target:axis.value};}
    if(!binding)return;
    const duplicate=this.profile.buttons.some(b=>b&&b.kind===binding.kind&&b.index===binding.index&&(b.kind==='button'||Math.abs(b.target-binding.target)<.1));
    if(duplicate){this.notice='That control is already assigned. Choose a different one, or skip this optional action.';return;}
    this.profile.buttons[s.button]=binding;this.next();
  }
  result(){
    const result=JSON.parse(JSON.stringify(this.profile));
    const hats=[12,13,14,15].map(i=>result.buttons[i]);
    if(hats.every(b=>b?.kind==='target'&&b.index===hats[0].index&&Math.abs(b.rest)>1.05)){
      const values=hats.map(b=>b.target).sort((a,b)=>a-b),step=(values[3]-values[0])/3;
      if(step>.1&&step<1&&values.every((v,i)=>Math.abs(v-values[0]-i*step)<.03))
        for(const b of hats){b.period=step*4;b.tolerance=step*.52;}
    }
    return result;
  }
}
