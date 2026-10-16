import {controllerKey,snapshot,ControllerProfiles,ControllerCalibration,SETUP_STEPS} from './controller-mapping.mjs';
import {readPad} from './input.mjs';
export class ControllerSetup {
  constructor(hooks,env=globalThis){
    this.hooks=hooks;this.env=env;this.nodes=hooks.nodes;this.pads=[];this.pad=null;
    this.selectedIndex=null;this.selectedKey=null;this.calibration=null;this.revision=0;this.quiet=false;this.optionKey='';this.resultMessage='';
    let storage;try{storage=env.localStorage;}catch{}this.profiles=new ControllerProfiles(storage);
    const n=this.nodes;n.open.onclick=()=>this.open();n.close.onclick=()=>n.dialog.close();
    n.device.onchange=()=>{const p=this.pads.find(p=>p.index===Number(n.device.value));if(p){this.selectedIndex=p.index;this.selectedKey=controllerKey(p);this.pad=p;this.calibration=null;this.resultMessage='';this.revision++;}};
    n.begin.onclick=()=>{if(this.pad){this.calibration=new ControllerCalibration(this.pad);this.resultMessage='';}};
    n.back.onclick=()=>this.calibration?.back();n.skip.onclick=()=>this.calibration?.skip();
    n.save.onclick=()=>this.save();
    n.reset.onclick=()=>{if(this.pad){this.profiles.remove(this.pad);this.calibration=null;this.quiet=true;this.revision++;this.resultMessage='Saved setup cleared. You can configure this controller again.';}};
    n.deadzone.oninput=()=>{
      const value=Number(n.deadzone.value)/100;
      if(this.calibration)this.calibration.profile.deadzone=value;
      else if(this.pad&&this.profile(this.pad)){this.profiles.save(this.pad,{...this.profile(this.pad),deadzone:value});this.revision++;}
    };
    n.dialog.addEventListener('close',()=>{this.calibration=null;this.quiet=true;this.revision++;});
  }
  get active(){return this.nodes.dialog.open;}
  profile(pad){return this.profiles.get(pad);}
  open(){this.hooks.pause();this.calibration=null;this.resultMessage='';this.nodes.dialog.showModal();this.optionKey='';this.render();}
  save(){
    if(!this.pad||!this.calibration?.canSave)return;
    const stored=this.profiles.save(this.pad,this.calibration.result());this.calibration=null;this.quiet=true;this.revision++;
    this.resultMessage=stored?'Setup saved for this controller in this browser. Test the sticks and buttons below, then close to play.':'Setup is ready for this session. Test the controls below, then close to play.';
  }
  poll(pads,time){
    this.pads=pads.filter(p=>p.connected&&p.mapping!=='xr-standard');
    let p;
    if(this.calibration)p=this.pads.find(p=>controllerKey(p)===this.calibration.key);
    else p=this.pads.find(p=>p.index===this.selectedIndex&&controllerKey(p)===this.selectedKey)||
      this.pads.find(p=>controllerKey(p)===this.selectedKey)||
      this.pads.find(p=>p.mapping==='standard'||this.profile(p))||this.pads[0];
    const stamp=p?`${p.index}:${controllerKey(p)}`:null;
    if(stamp!==this.activeStamp){
      if(this.activeStamp!==undefined){this.quiet=true;this.revision++;this.hooks.pause('Controller changed. Release its controls, then resume.');}
      this.activeStamp=stamp;
    }
    this.pad=p||null;this.selectedIndex=p?.index??null;if(p)this.selectedKey=controllerKey(p);
    if(this.active){this.calibration?.poll(this.pad,time);this.render();}
    if(this.quiet&&p){const input=readPad(p,this.profile(p));if(input&&!input.buttons.some(Boolean)&&Math.max(Math.abs(input.mx),Math.abs(input.my),Math.abs(input.ax),Math.abs(input.ay))<.1)this.quiet=false;}
    return this.pad;
  }
  render(){
    const n=this.nodes,p=this.pad,c=this.calibration;
    const key=JSON.stringify(this.pads.map(p=>[p.index,controllerKey(p)]));
    if(key!==this.optionKey){
      this.optionKey=key;n.device.replaceChildren();
      for(const pad of this.pads){const option=this.env.document.createElement('option');option.value=String(pad.index);option.textContent=`${pad.id||'Game controller'} · ${pad.mapping==='standard'?'browser layout':'custom layout'}`;n.device.append(option);}
    }
    n.device.value=p?String(p.index):'';n.device.disabled=!this.pads.length;
    n.begin.disabled=!p;n.reset.disabled=!p||!this.profile(p);n.back.disabled=!c||c.index===0;
    n.skip.disabled=!c||c.done||!!c.step?.required;n.save.disabled=!p||!c?.canSave;
    n.deadzone.disabled=!c&&!this.profile(p);n.deadzone.value=Math.round((c?.profile.deadzone||this.profile(p)?.deadzone||.16)*100);
    n.deadzoneValue.textContent=`${n.deadzone.value}%`;
    const prompt=!p?'Pair your controller in macOS, then press a controller button.':c?
      c.done?'Controls learned. Test them below, then save.':`${c.index+1} / ${SETUP_STEPS.length} — ${c.step.prompt}`:
      this.resultMessage||'Release both sticks and triggers, then choose Begin setup.';
    if(n.prompt.textContent!==prompt)n.prompt.textContent=prompt;
    n.instructions.textContent=c&&!c.done?(c.notice|| (c.phase==='release'?'Release all controls and centre the sticks before the next input.':'Make only the requested input. Optional controls can be skipped.')):
      'Standard controllers work automatically. Configure only if the mapping is missing or incorrect. Bluetooth and USB layouts are saved separately.';
    if(!p){n.raw.textContent='No controller detected by this browser.';n.preview.textContent='';return;}
    const raw=snapshot(p);
    n.raw.textContent=`Axes: ${raw.axes.map((v,i)=>`${i}: ${v.toFixed(2)}`).join(' · ')}\nPressed buttons: ${raw.buttons.map((v,i)=>v>.5?i:null).filter(v=>v!==null).join(', ')||'none'}`;
    const profile=c?.canSave?c.result():this.profile(p),input=c&&!c.canSave?null:readPad(p,profile);
    n.preview.textContent=input?`Move ${input.mx.toFixed(2)}, ${input.my.toFixed(2)} · Aim ${input.ax.toFixed(2)}, ${input.ay.toFixed(2)}\nFire ${input.fire?'ON':'off'} · Camera ${input.orbit} · Confirm ${input.buttons[0]?'ON':'off'} · Pause ${input.buttons[9]?'ON':'off'}`:'Finish the required controls to test the learned mapping. If there is no right stick, skip its two steps; firing still has aim assistance.';
  }
}
