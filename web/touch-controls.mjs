import {stick} from './input.mjs';
const preferenceKey='domeblast.touch.v1';
export class TouchControls {
  constructor(hooks,env=globalThis){
    this.hooks=hooks;this.env=env;this.nodes=hooks.nodes;this.playing=false;
    this.sticks=[{id:null,x:0,y:0,node:this.nodes.left,nub:this.nodes.leftNub},
      {id:null,x:0,y:0,node:this.nodes.right,nub:this.nodes.rightNub}];
    this.orbits=[new Set(),new Set()];
    const capable=(env.navigator?.maxTouchPoints||0)>0||!!env.matchMedia?.('(any-pointer: coarse)').matches;
    this.enabled=capable;
    try{const saved=env.localStorage?.getItem(preferenceKey);if(saved==='on'||saved==='off')this.enabled=saved==='on';}catch{}
    this.nodes.game.classList.toggle('touch-device',capable);
    for(const state of this.sticks){
      state.node.addEventListener('pointerdown',e=>{
        if(!this.enabled||!this.playing||state.id!==null||(e.pointerType==='mouse'&&e.button!==0))return;
        e.preventDefault();this.hooks.unlock();state.id=e.pointerId;
        const rect=state.node.getBoundingClientRect();state.cx=rect.left+rect.width/2;state.cy=rect.top+rect.height/2;state.radius=Math.max(1,Math.min(rect.width,rect.height)*.3);
        try{state.node.setPointerCapture(e.pointerId);}catch{}
        state.node.classList.add('held');this.move(state,e);
      });
      state.node.addEventListener('pointermove',e=>{if(e.pointerId===state.id){e.preventDefault();this.move(state,e);}});
      for(const event of ['pointerup','pointercancel','lostpointercapture'])state.node.addEventListener(event,e=>{if(e.pointerId===state.id)this.release(state);});
      state.node.addEventListener('contextmenu',e=>e.preventDefault());
    }
    [this.nodes.orbitLeft,this.nodes.orbitRight].forEach((node,index)=>{
      node.addEventListener('pointerdown',e=>{
        if(!this.enabled||!this.playing||(e.pointerType==='mouse'&&e.button!==0))return;
        e.preventDefault();this.hooks.unlock();this.orbits[index].add(e.pointerId);
        try{node.setPointerCapture(e.pointerId);}catch{}node.classList.add('held');
      });
      for(const event of ['pointerup','pointercancel','lostpointercapture'])node.addEventListener(event,e=>{
        this.orbits[index].delete(e.pointerId);if(!this.orbits[index].size)node.classList.remove('held');
      });
      node.addEventListener('contextmenu',e=>e.preventDefault());
    });
    this.nodes.auto.onclick=()=>{if(this.playing)this.hooks.toggleAuto();};
    this.nodes.toggle.onclick=()=>{this.setEnabled(!this.enabled);try{env.localStorage?.setItem(preferenceKey,this.enabled?'on':'off');}catch{}};
    env.addEventListener?.('blur',()=>this.reset());env.addEventListener?.('resize',()=>this.reset());
    env.document?.addEventListener('visibilitychange',()=>{if(env.document.hidden)this.reset();});
    this.setEnabled(this.enabled);
  }
  setEnabled(enabled){
    this.enabled=!!enabled;if(!this.enabled)this.reset();
    this.nodes.game.classList.toggle('touch-enabled',this.enabled);
    this.nodes.toggle.textContent=`Touch controls: ${this.enabled?'On':'Off'}`;
    this.nodes.toggle.setAttribute('aria-pressed',String(this.enabled));this.update(this.playing);
  }
  update(playing,autoFire){
    this.playing=!!playing;
    const visible=this.enabled&&this.playing;
    if(!visible)this.reset();
    this.nodes.root.hidden=!visible;this.nodes.game.classList.toggle('touch-playing',visible);
    if(autoFire!==undefined){
      const label=`AUTO ${autoFire?'ON':'OFF'}`;
      if(this.nodes.auto.textContent!==label)this.nodes.auto.textContent=label;
      this.nodes.auto.setAttribute('aria-pressed',String(!!autoFire));
    }
  }
  move(state,e){
    let x=(e.clientX-state.cx)/state.radius,y=(e.clientY-state.cy)/state.radius;
    if(!Number.isFinite(x)||!Number.isFinite(y)){x=y=0;}
    const magnitude=Math.hypot(x,y);if(magnitude>1){x/=magnitude;y/=magnitude;}
    [state.x,state.y]=stick(x,y,.08);
    state.nub.style.transform=`translate3d(${x*state.radius}px,${y*state.radius}px,0)`;
  }
  release(state){
    const id=state.id;state.id=null;state.x=state.y=0;
    state.nub.style.transform='translate3d(0,0,0)';state.node.classList.remove('held');
    if(id!==null)try{state.node.releasePointerCapture(id);}catch{}
  }
  reset(){
    for(const state of this.sticks)this.release(state);
    [this.nodes.orbitLeft,this.nodes.orbitRight].forEach((node,i)=>{
      const ids=[...this.orbits[i]];this.orbits[i].clear();node.classList.remove('held');
      for(const id of ids)try{node.releasePointerCapture(id);}catch{}
    });
  }
  read(){
    const [left,right]=this.sticks,active=this.enabled&&this.playing;
    return {mx:active?left.x:0,my:active?left.y:0,ax:active?right.x:0,ay:active?right.y:0,
      moveActive:active&&left.id!==null,aimActive:active&&right.id!==null,
      fire:active&&right.id!==null,orbitActive:active&&(this.orbits[0].size>0||this.orbits[1].size>0),orbit:active?Number(this.orbits[1].size>0)-Number(this.orbits[0].size>0):0};
  }
}
