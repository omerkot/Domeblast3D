import {tableTransform,inverseTable,rotateTable,matricesForView,readXRInput} from './xr-math.mjs';
import {edges,VR_QUALITY} from './input.mjs';

const sectorNames=['First Light','Ion Blue','Violet Storm','Turquoise Horizon','Silver Veil','Midnight Infinity'];
export class TabletopVR {
  constructor(hooks,env=globalThis) {
    this.hooks=hooks;this.env=env;this.supported=false;this.session=null;this.entering=false;
    this.selected=0;this.previous=[];this.primed=false;this.lastScreen=-1;this.lastTime=0;
    this.framebufferId=0;this.textureId=0;this.framebuffer=null;this.texture=null;
    this.model=null;this.anchor=null;this.anchorInverse=null;this.tableYaw=0;this.recenterPending=false;this.message='';this.navigation=[0,0];this.repeatAt=[0,0];
    this.startupTimer=null;this.submittedFrames=0;this.lastFailure='';this.stage='idle';
    this.failedQualities=new Set();this.qualityNotice='';this.probeLayer=null;this.probeFrames=0;
    this.layerProfiles=new WeakMap();this.graphicsOpen=false;this.colorChoice='';this.hdrSupported=false;
    this.fps=0;this.rateStart=0;this.rateFrames=0;this.eyeResolution='';
    this.button=hooks.button;
    if(hooks.graphicsControls){
      hooks.graphicsControls.quality.onchange=e=>this.setQuality(e.target.value);
      hooks.graphicsControls.colors.onchange=e=>this.setColors(e.target.value);
    }
    this.button.hidden=true;this.button.onclick=()=>this.enter();
    this.onDeviceChange=()=>this.detect();
    env.navigator?.xr?.addEventListener?.('devicechange',this.onDeviceChange);
  }
  get active(){return !!this.session||this.entering;}
  async detect() {
    let supported=false;
    try{supported=!!this.env.isSecureContext&&!!this.env.XRWebGLLayer&&
      !!(await this.env.navigator?.xr?.isSessionSupported('immersive-vr'));}catch{}
    this.supported=supported;this.button.hidden=!supported;
    this.button.disabled=!this.hooks.ready()||this.entering;
    if(this.hooks.graphicsControls){
      const controls=this.hooks.graphicsControls,settings=this.hooks.settings();
      controls.group.hidden=!supported;controls.quality.value=settings.vrQuality||'high';
      controls.colors.value=settings.vrColors||'enhanced';
    }
    return supported;
  }
  quality(){return VR_QUALITY[this.hooks.settings().vrQuality]||VR_QUALITY.high;}
  checkGL(stage,force=false){
    if(!force&&this.probeFrames<=0)return;
    const code=this.gl.getError();
    if(typeof code==='number'&&code!==0)throw Object.assign(new Error(`WebGL error ${code} during ${stage}`),{graphicsFault:true,webglCode:code});
  }
  drainGLErrors(){for(let i=0;i<8;i++){const code=this.gl.getError();if(typeof code!=='number'||code===0)break;}}
  recoverHigh(error,profile){
    if(!error.graphicsFault||profile!==VR_QUALITY.high||this.failedQualities.has('high')||this.gl.isContextLost())return false;
    this.failedQualities.add('high');this.drainGLErrors();
    if(!this.setQuality('balanced')){this.hooks.setGraphics({vrQuality:'balanced'});error.message+='; Balanced selected for the next VR start';return false;}
    this.qualityNotice='High was rejected by the headset. Using Balanced.';
    this.hooks.pause(this.qualityNotice);this.graphicsOpen=false;this.selected=0;this.hudKey='';
    return true;
  }
  makeLayer(profile){
    const layer=new this.env.XRWebGLLayer(this.session,this.gl,{alpha:false,antialias:true,
      depth:true,framebufferScaleFactor:profile.scale});
    if('fixedFoveation' in layer)layer.fixedFoveation=profile.foveation;
    this.checkGL('headset layer creation',true);
    this.layerProfiles.set(layer,profile);return layer;
  }
  setQuality(id){
    if(!Object.hasOwn(VR_QUALITY,id))return false;
    if(this.session&&this.failedQualities.has(id)){this.qualityNotice='That quality is unavailable in this VR session.';this.hudKey='';return false;}
    if(this.session){
      try{const layer=this.makeLayer(VR_QUALITY[id]);this.session.updateRenderState({baseLayer:layer});this.layer=layer;}
      catch{this.failedQualities.add(id);this.drainGLErrors();this.qualityNotice='That quality is unavailable; keeping the current setting.';this.hudKey='';return false;}
    }
    this.hooks.setGraphics({vrQuality:id});this.qualityNotice='';this.hudKey='';return true;
  }
  cycleQuality(direction){
    const ids=Object.keys(VR_QUALITY).filter(id=>!this.session||!this.failedQualities.has(id)),index=ids.indexOf(this.hooks.settings().vrQuality||'high');
    this.setQuality(ids[(index+direction+ids.length)%ids.length]);
  }
  setColors(id){
    if(!['original','enhanced'].includes(id))return;
    this.hooks.setGraphics({vrColors:id});this.probeFrames=3;this.hudKey='';
  }
  applyColors(){
    const choice=this.hooks.settings().vrColors||'enhanced';
    if(choice===this.colorChoice)return;
    this.hooks.module()._web_xr_color(choice==='original'?0:this.hdrSupported?2:1);
    this.colorChoice=choice;
  }
  async enter() {
    if(this.active||!this.supported||!this.hooks.ready())return;
    this.entering=true;this.button.disabled=true;this.message='';this.lastFailure='';this.submittedFrames=0;this.rateRequested=false;this.anchoredWithPosition=false;this.stage='session request';this.graphicsOpen=false;this.colorChoice='';this.fps=0;this.rateStart=0;this.rateFrames=0;this.hooks.unlock();
    this.failedQualities.clear();this.qualityNotice='';this.probeLayer=null;this.probeFrames=0;
    this.hooks.onEntering();
    let session;
    try {
      // Keep requestSession in the actual Enter VR click's activation scope.
      session=await this.env.navigator.xr.requestSession('immersive-vr',{optionalFeatures:['local-floor']});
      this.session=session;this.stage='graphics setup';
      this.startupTimer=this.env.setTimeout?.(()=>{
        if(this.session===session&&this.submittedFrames===0){
          this.lastFailure=`VR did not start drawing (${this.stage}). Please retry Enter VR.`;
          this.exit();
        }
      },10000);
      session.addEventListener('end',()=>this.cleanup(session));
      session.addEventListener('visibilitychange',()=>{
        if(session.visibilityState!=='visible')this.hooks.pause('VR paused while the headset is away.');
        this.lastTime=0;this.primed=false;
      });
      const gl=this.hooks.module().ctx;this.gl=gl;
      if(!gl||gl.isContextLost())throw Error('Graphics are unavailable.');
      await gl.makeXRCompatible();
      if(this.session!==session)throw Error('VR session ended.');
      this.hdrSupported=!!gl.getExtension('EXT_color_buffer_float');
      this.maxRenderEdge=Math.min(...[gl.getParameter(gl.MAX_TEXTURE_SIZE),gl.getParameter(gl.MAX_RENDERBUFFER_SIZE),2048]
        .filter(value=>Number.isFinite(value)&&value>0));
      try{this.layer=this.makeLayer(this.quality());}
      catch(error){
        const preferred=this.hooks.settings().vrQuality||'high';this.failedQualities.add(preferred);this.drainGLErrors();
        if(preferred==='performance')throw error;
        const fallback=preferred==='high'?'balanced':'performance';
        this.layer=this.makeLayer(VR_QUALITY[fallback]);this.hooks.setGraphics({vrQuality:fallback});
        this.qualityNotice=`Using ${VR_QUALITY[fallback].label}; the requested quality was unavailable.`;
      }
      this.applyColors();
      session.updateRenderState({baseLayer:this.layer,depthNear:.02,depthFar:20});
      this.stage='headset tracking setup';
      try{this.space=await session.requestReferenceSpace('local-floor');}
      catch{this.space=await session.requestReferenceSpace('local');}
      if(this.session!==session)throw Error('VR session ended.');
      this.space.addEventListener?.('reset',()=>{this.recenterPending=true;this.hooks.pause('Recentered. Resume when ready.');});
      // Start drawing before asking the XR runtime to change its frame rate.
      if(this.session!==session)throw Error('VR session ended.');
      this.canvas=this.env.document.createElement('canvas');this.canvas.width=1024;this.canvas.height=768;
      this.ctx2d=this.canvas.getContext('2d');
      if(!this.ctx2d)throw Error('VR menu graphics are unavailable.');
      this.texture=gl.createTexture();
      if(!this.texture)throw Error('VR menu texture is unavailable.');
      gl.activeTexture(gl.TEXTURE0);gl.bindTexture(gl.TEXTURE_2D,this.texture);
      gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE);
      this.textureId=this.hooks.module().registerXRTexture(this.texture);
      this.matrixPointer=this.hooks.module()._web_xr_buffer();
      this.model=null;this.anchor=null;this.anchorInverse=null;this.tableYaw=0;this.primed=false;this.lastTime=0;this.lastScreen=-1;this.hudKey='';
      this.hooks.module()._web_xr_mode(1);
      this.entering=false;this.stage='first headset frame';this.hooks.onEntered();
      this.frameHandle=session.requestAnimationFrame((time,frame)=>this.frame(time,frame));
    } catch(error) {
      this.lastFailure=this.lastFailure||`VR could not start (${this.stage}: ${error.message||error}). You can retry or play on screen.`;
      if(session){try{await session.end();}catch{}this.cleanup(session);}
      else {this.entering=false;this.button.disabled=false;this.hooks.onLeft();}
      this.hooks.feedback(this.lastFailure);
    }
  }
  async exit(){if(this.session)try{await this.session.end();}catch{this.cleanup(this.session);}}
  cleanup(session) {
    if(this.session!==session)return;
    this.session=null;this.entering=false;this.env.clearTimeout?.(this.startupTimer);this.startupTimer=null;
    const mod=this.hooks.module();
    if(this.framebufferId)mod.releaseXRFramebuffer(this.framebufferId);
    if(this.textureId)mod.releaseXRTexture(this.textureId);
    if(this.texture)this.gl.deleteTexture(this.texture);
    this.framebufferId=this.textureId=0;this.framebuffer=this.texture=null;
    mod._web_xr_mode(0);this.model=null;this.anchor=null;this.anchorInverse=null;this.tableYaw=0;this.previous=[];this.primed=false;
    this.button.disabled=false;this.hooks.onLeft();
    if(this.lastFailure)this.hooks.feedback(this.lastFailure);
  }
  items() {
    const state=this.hooks.state(),settings=this.hooks.settings(),mod=this.hooks.module();
    const mode=mod._web_stat(5);
    const auto={label:'Autofire',value:settings.autoFire?'On':'Off',run:()=>this.hooks.action(3)};
    const exit={label:'Exit VR',run:()=>this.exit()};
    if(this.graphicsOpen)return [
      {label:'Quality',value:this.quality().label,adjust:d=>this.cycleQuality(d)},
      {label:'Colors',value:(settings.vrColors||'enhanced')==='original'?'Original':'Enhanced',
        run:()=>this.setColors(settings.vrColors==='original'?'enhanced':'original')},
      {label:'Back',run:()=>{this.graphicsOpen=false;this.selected=0;}},
    ];
    const graphics={label:'Graphics',value:this.quality().label,run:()=>{this.graphicsOpen=true;this.selected=0;}};
    if(state.screen===0)return [
      {label:mode?'Launch practice':'Launch arcade',run:()=>this.hooks.action(0)},
      {label:'Mode',value:mode?`Practice ${mode} / ${sectorNames[mode-1]}`:'Arcade',adjust:d=>this.hooks.changeSector(d)},
      auto,{label:'Cadence',value:settings.classic?'Slow':'Standard',run:()=>this.hooks.action(5)},
      {label:'Music',value:settings.muted?'Off':'On',run:()=>this.hooks.toggleMusic()},graphics,exit];
    if(state.screen===3)return [
      {label:'Deploy again',run:()=>this.hooks.action(0)},
      {label:'Return to title',run:()=>this.hooks.action(2)},exit];
    return [
      {label:'Resume',run:()=>this.hooks.action(0)},
      ...['music','effects','master'].map((key,index)=>({label:['Music volume','Sound effects','Master volume'][index],
        value:`${settings[key]}%`,adjust:d=>this.hooks.setAudio(key,Math.min(100,Math.max(0,settings[key]+d*5)))})),
      auto,graphics,{label:'Recenter table',run:()=>{this.recenterPending=true;}},
      {label:'Return to title',run:()=>this.hooks.action(2)},exit];
  }
  input(input,time) {
    if(!input.both){this.primed=false;this.hooks.pause('Connect both Quest controllers to resume.');this.message='Connect both Quest controllers';return;}
    this.message='';
    const pressed=this.primed?edges(input.buttons,this.previous):[];
    this.previous=input.buttons;this.primed=true;
    const state=this.hooks.state();
    if(pressed[3])this.recenterPending=true;
    if(pressed[2])this.hooks.action(3);
    if(pressed[1]) {
      if(this.graphicsOpen){this.graphicsOpen=false;this.selected=0;return;}
      if(state.screen===1)this.hooks.pause();
      else if(state.screen===2)this.hooks.action(0);
      else if(state.screen===3)this.hooks.action(2);
      else this.exit();
      this.selected=0;return;
    }
    if(state.screen===1)return;
    const items=this.items();
    const dirs=[Math.abs(input.my)>.55?Math.sign(input.my):0,Math.abs(input.mx)>.55?Math.sign(input.mx):0];
    dirs.forEach((direction,axis)=>{
      if(direction&&(direction!==this.navigation[axis]||time>=this.repeatAt[axis])){
        if(axis===0)this.selected=(this.selected+direction+items.length)%items.length;
        else items[this.selected]?.adjust?.(direction);
        this.repeatAt[axis]=time+250;
      }
      this.navigation[axis]=direction;
    });
    if(pressed[0]||pressed[4]){
      const item=items[this.selected]||items[0];
      if(item.run)item.run();else item.adjust?.(1);
    }
  }
  drawHud() {
    const state=this.hooks.state(),settings=this.hooks.settings(),compact=state.screen===1;
    const items=compact?[]:this.items();
    const key=JSON.stringify([state.screen,state.score,state.peak,state.lives,state.level,state.dying,state.locked,
      state.practice,settings,this.selected,this.message,this.qualityNotice,this.graphicsOpen,this.graphicsOpen?this.fps:0,this.eyeResolution,items.map(i=>[i.label,i.value])]);
    if(key===this.hudKey)return;
    this.hudKey=key;const hudScale=this.quality().hudScale;
    this.canvas.width=1024*hudScale;this.canvas.height=(compact?256:768)*hudScale;
    const c=this.ctx2d,w=1024,h=compact?256:768;c.scale(hudScale,hudScale);
    c.clearRect(0,0,w,h);c.fillStyle='rgba(4,13,23,.95)';c.fillRect(0,0,w,h);
    c.fillStyle='#ff9b45';c.fillRect(0,0,6,h);
    const line=(text,x,y,size=32,color='#eaf8ff')=>{c.font=`${size}px sans-serif`;c.fillStyle=color;c.fillText(text,x,y,c.textAlign==='right'?520:w-x-25);};
    if(compact){
      line(`SCORE ${state.score.toLocaleString()}     CANNONS ${state.lives}     SECTOR ${state.level}`,35,65,36);
      line(state.dying?'CANNON LOST — DEPLOYING RESERVE':state.locked?'TARGET LOCKED':sectorNames[state.level-1].toUpperCase(),35,126,32,state.locked?'#ff9b45':'#8cddf3');
      line('SIDE GRIPS: ROTATE ARENA',35,163,25,'#a4bdcd');
      line(`AUTO ${state.autoFire?'ON':'OFF'}  /  B PAUSE  /  X AUTOFIRE  /  Y RECENTER`,35,205,27,'#a4bdcd');
    }else{
      line('DOMEBLAST 3D / TABLETOP VR',35,63,42,'#8cddf3');
      line(this.message||this.qualityNotice||(this.graphicsOpen?'GRAPHICS':state.screen===0?'Choose your flight':state.screen===3?`SIGNAL LOST / PEAK ${state.peak.toLocaleString()}`:'PAUSED'),35,119,31,'#a4bdcd');
      items.forEach((item,index)=>{
        const y=165+index*57;
        if(index===this.selected){c.fillStyle='#153747';c.fillRect(25,y-34,w-50,51);}
        line(`${index===this.selected?'›':' '} ${item.label}`,42,y,32);
        if(item.value){c.textAlign='right';line(item.value,w-42,y,27,'#ffb56b');c.textAlign='left';}
      });
      if(this.graphicsOpen){
        line('High is sharpest. Try Balanced if movement stutters.',35,495,29,'#a4bdcd');
        line(`PER EYE: ${this.eyeResolution||'Measuring…'}`,35,560,28,'#8cddf3');
        line(`FRAME RATE: ${this.fps?`~${this.fps} fps`:'Measuring…'}`,35,610,28,'#8cddf3');
      }
      line('LEFT STICK: SELECT / ADJUST     A OR TRIGGER: CONFIRM',35,690,26,'#a4bdcd');
      line('MOVE / AIM: STICKS · FIRE: RIGHT TRIGGER · TURN: SIDE GRIPS',35,736,24,'#8cddf3');
    }
    const gl=this.gl;gl.activeTexture(gl.TEXTURE0);gl.bindTexture(gl.TEXTURE_2D,this.texture);
    gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL,false);gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL,false);
    gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,gl.RGBA,gl.UNSIGNED_BYTE,this.canvas);
  }
  writeMatrices(values){this.hooks.module().HEAPF32.set(values,this.matrixPointer/4);}
  frame(time,frame) {
    const session=this.session;
    if(!session||frame.session!==session)return;
    this.frameHandle=session.requestAnimationFrame((t,f)=>this.frame(t,f));
    let profile;
    try{
      const layer=session.renderState?.baseLayer||this.layer;
      profile=this.layerProfiles.get(layer)||this.quality();
      if(layer!==this.probeLayer){this.probeLayer=layer;this.probeFrames=3;}
      this.checkGL('frame setup');
      const pose=frame.getViewerPose(this.space);
      const dt=this.lastTime?Math.min(.05,Math.max(0,(time-this.lastTime)/1000)):0;this.lastTime=time;
      if(!pose||!pose.views.length){this.hooks.pause('Headset tracking lost. Resume when ready.');this.primed=false;return;}
      if(!this.model||this.recenterPending||(!this.anchoredWithPosition&&!pose.emulatedPosition)){this.anchor=tableTransform(pose.transform.matrix);this.anchorInverse=inverseTable(this.anchor);this.tableYaw=0;this.model=this.anchor;this.inverseModel=this.anchorInverse;this.recenterPending=false;this.anchoredWithPosition=!pose.emulatedPosition;}
      const tracked=Array.from(session.inputSources).filter(source=>{
        const space=source.gripSpace||source.targetRaySpace;
        return space&&!!frame.getPose(space,this.space);
      });
      this.stage='rendering';
      if(session.visibilityState==='visible'){
        if(!this.rateStart)this.rateStart=time;
        this.rateFrames++;
        if(time-this.rateStart>=1000){this.fps=Math.round(this.rateFrames*1000/(time-this.rateStart));this.rateStart=time;this.rateFrames=0;}
      }else{this.rateStart=0;this.rateFrames=0;}

      const input=readXRInput(tracked);
      if(session.visibilityState==='visible')this.input(input,time);
      else{this.hooks.pause('VR paused while the headset is away.');this.primed=false;}
      if(this.session!==session)return;
      const mod=this.hooks.module();
      if(input.both&&session.visibilityState==='visible'&&!pose.emulatedPosition&&!this.recenterPending&&
          mod._web_stat(0)===1&&!this.hooks.state().dying&&input.orbit!==0){
        // Match the native shoulder-button orbit rate. Rotating the model in the
        // opposite direction gives the same viewing direction without moving the head.
        this.tableYaw=((this.tableYaw-input.orbit*.9*dt+Math.PI*3)%(Math.PI*2))-Math.PI;
        this.model=rotateTable(this.anchor,this.tableYaw);this.inverseModel=inverseTable(this.model);
      }
      this.writeMatrices(matricesForView(pose.views[0].projectionMatrix,pose.transform,this.model,this.inverseModel));
      this.applyColors();this.checkGL('scene color setup');
      mod._web_xr_pose();
      mod._web_update(session.visibilityState==='visible'?dt:0,input.mx,input.my,input.ax,input.ay,0,+input.fire,+input.focus);
      const screen=this.hooks.state().screen;
      if(screen!==this.lastScreen){this.selected=0;this.lastScreen=screen;this.navigation=[0,0];this.graphicsOpen=false;}
      this.drawHud();this.checkGL('HUD texture upload');
      const fb=layer.framebuffer;
      if(!fb)throw Error('No headset framebuffer');
      const gl=this.gl;
      gl.bindFramebuffer(gl.FRAMEBUFFER,fb);
      if(this.probeFrames>0){
        const status=gl.checkFramebufferStatus(gl.FRAMEBUFFER);
        if(typeof status==='number'&&status!==gl.FRAMEBUFFER_COMPLETE)throw Object.assign(new Error(`Incomplete headset framebuffer ${status}`),{graphicsFault:true});
      }
      gl.disable(gl.SCISSOR_TEST);gl.colorMask(true,true,true,true);gl.depthMask(true);
      gl.clearColor(.008,.012,.025,1);gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);this.checkGL('headset framebuffer clear');
      if(fb!==this.framebuffer){
        if(this.framebufferId)mod.releaseXRFramebuffer(this.framebufferId);
        this.framebuffer=fb;this.framebufferId=mod.registerXRFramebuffer(fb);
      }
      let renderedEyes=0;
      for(const view of pose.views){
        const viewport=layer.getViewport(view);if(!viewport)continue;
        this.writeMatrices(matricesForView(view.projectionMatrix,view.transform,this.model,this.inverseModel));
        mod.HEAPF32.set(matricesForView(view.projectionMatrix,view.transform,this.anchor,this.anchorInverse),this.matrixPointer/4+32);
        const scale=Math.min(1,Math.min(profile.maxEdge,this.maxRenderEdge)/Math.max(viewport.width,viewport.height));
        if(renderedEyes===0)this.eyeResolution=`${Math.round(viewport.width*scale)} × ${Math.round(viewport.height*scale)}`;
        mod._web_xr_draw(this.framebufferId,viewport.x,viewport.y,viewport.width,viewport.height,
          Math.max(1,Math.round(viewport.width*scale)),Math.max(1,Math.round(viewport.height*scale)),this.textureId);
        this.checkGL(`${view.eye||'eye'} rendering`);
        renderedEyes++;
      }
      if(!renderedEyes)throw Error('No headset eye viewports');
      if(this.probeFrames>0)this.probeFrames--;
      this.submittedFrames++;this.stage='playing';
      this.env.clearTimeout?.(this.startupTimer);this.startupTimer=null;
      // Optional tuning must never gate the render loop: some runtimes settle
      // this promise only after frames have started reaching the compositor.
      if(!this.rateRequested&&session.supportedFrameRates?.includes(72)){
        this.rateRequested=true;
        try{Promise.resolve(session.updateTargetFrameRate(72)).catch(()=>{});}catch{}
      }
    }catch(error){if(this.recoverHigh(error,profile))return;this.lastFailure=`VR stopped (${error.message||error}). Your run is paused; retry Enter VR.`;this.exit();}
  }
}
