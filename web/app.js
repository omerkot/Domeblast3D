import {readPad,edges,sanitizeSave} from './input.mjs';
import {TabletopVR} from './xr.mjs';
import {ControllerSetup} from './controller-setup.mjs';
import {TouchControls} from './touch-controls.mjs';
import {protectGameSelection,setupFullscreen} from './browser-display.mjs';
const $=id=>document.getElementById(id), names=['First Light','Ion Blue','Violet Storm','Turquoise Horizon','Silver Veil','Midnight Infinity'];
const tracks=['Ignition Bloom','Blue Shift Boulevard','Violet Velocity','Aurora Afterburn','Silver Overdrive','Midnight Homecoming'];
let ready=false,state={screen:0},saved=sanitizeSave(),lastSave='',previous=[],padIndex=null,keys=new Set(),lastTime=0,raf=0,pauseRow=0,lastScreen=-1,focused=true,audioWasRunning=false,musicActivationGesture=false,controllerRevision=0,touch=null;
try {saved=sanitizeSave(JSON.parse(localStorage.getItem('domeblast.web.v1')||'{}'));}catch{}
const radar=$('radar').getContext('2d');
function text(id,value){if($(id).textContent!==String(value))$(id).textContent=value;}
function persist(){const s=JSON.stringify(saved);if(s===lastSave)return;try{localStorage.setItem('domeblast.web.v1',s);lastSave=s;}catch{}}
function action(id,value=0){if(!ready)return;if([0,1,4,8].includes(id))unlock();Module._web_action(id,value);saved.autoFire=!!Module._web_stat(6);saved.classic=!!Module._web_stat(7);saved.muted=!!Module._web_stat(8);persist();updateOptions();}
function updateOptions(){const sector=ready?Module._web_stat(5):0;text('mode',sector?`PRACTICE ${String(sector).padStart(2,'0')} · ${names[sector-1].toUpperCase()}`:'ARCADE · START AT ZERO');$('launch').firstChild.textContent=sector?'Launch practice ':'Launch arcade ';text('auto-toggle',`Autofire ${saved.autoFire?'on':'off'}`);text('cadence-toggle',saved.classic?'Slow cadence':'Standard cadence');updateAudioStatus();}
function changeSector(direction){action(6,(Module._web_stat(5)+direction+7)%7);}
// Capture the gesture even while the 18 MB asset bundle is still loading.
// SDL reuses this exact context when it opens its output device.
function unlock(){
  try {
    Module.SDL2 ??= {};
    if(!Module.SDL2.audioContext){
      const Context=window.AudioContext||window.webkitAudioContext;
      if(!Context)return;
      Module.SDL2.audioContext=new Context();
    }
    const ctx=Module.SDL2.audioContext;
    if(ctx.state!=='running'&&ctx.state!=='closed')Promise.resolve(ctx.resume()).catch(()=>{});
  }catch{/* Audio activation can be retried on the next real gesture. */}
}
function audioRunning(){return Module.SDL2?.audioContext?.state==='running';}
function updateAudioStatus(){
  const running=audioRunning();audioWasRunning=running;
  $('audio-unlock').hidden=!ready||running;
  text('sound-toggle',saved.muted?'Music off':running?'Music on':'Enable music');
}
function toggleMusic(){
  // A suspended context is not a mute preference. Activate it without muting the mixer.
  if(!saved.muted&&(!audioWasRunning||!audioRunning())){unlock();return;}
  action(4);
}
function pause(reason){touch?.reset();if(!ready)return;action(10);text('pause-reason',reason||'Take a breath. The arena can wait.');keys.clear();}
function fail(message){touch?.update(false);ready=false;cancelAnimationFrame(raf);$('loading').hidden=false;text('load-status',`The game could not start. ${message} Reload to try again.`);$('load-progress').hidden=true;}
window.Module={canvas:$('canvas'),onFailure:fail,
  setStatus(message){if(message){text('load-status',message.startsWith('Downloading')?'Loading arena and Six Skies soundtrack…':message);const match=message.match(/\((\d+(?:\.\d+)?)\/(\d+)\)/);if(match)$('load-progress').value=Number(match[1])/Number(match[2])*100;}},
  onAbort(){fail('Check your connection and WebGL support.');},
  onReady(){ready=true;Module._web_restore(saved.high,+saved.autoFire,+saved.classic,saved.master,saved.music,saved.effects,+saved.muted);$('loading').hidden=true;$('menu').hidden=false;for(const key of ['master','music','effects']){$(key).value=saved[key];text(`${key}-output`,`${saved[key]}%`);}unlock();updateOptions();resize();vr.detect();raf=requestAnimationFrame(frame);},
  onState(next){state=next;updateAudioStatus();touch.update(ready&&!vr.active&&!controllers.active&&!$('controls').open&&!$('fullscreen-help').open&&next.screen===1&&!next.dying,saved.autoFire);
    if(next.high>saved.high){saved.high=next.high;persist();} text('score',next.score.toLocaleString());text('best',saved.high.toLocaleString());text('lives',String(next.lives).padStart(2,'0'));text('sector-number',`SECTOR ${String(next.level).padStart(2,'0')}`);text('sector-name',names[next.level-1].toUpperCase());text('practice',next.practice?'PRACTICE':'');text('track-name',tracks[next.level-1].toUpperCase());text('lock',next.dying?'CANNON LOST':next.locked?'TARGET LOCKED':'AIM TO LOCK');text('auto-status',`AUTO FIRE ${next.autoFire?'ON':'OFF'}`);
    text('banner',next.screen!==1?'':next.dying?(next.lives?'CANNON LOST\nDeploying reserve':'FINAL CANNON'):next.banner>0?`SECTOR 0${next.level}\n${names[next.level-1].toUpperCase()}`:'');
    if(next.screen!==lastScreen){lastScreen=next.screen;$('menu').hidden=next.screen!==0;$('pause-menu').hidden=next.screen!==2;$('gameover').hidden=next.screen!==3;$('hud').hidden=next.screen===0;$('bottom-hud').hidden=next.screen!==1;if(next.screen===3){text('final-score',next.peak.toLocaleString());text('final-caption',next.practice?'PRACTICE · PEAK SCORE':'PEAK SCORE');}if(next.screen===2)selectAudio(0);if(next.screen===0)updateOptions();const focusId=['launch','canvas','resume','retry'][next.screen];if(!vr.active&&!controllers.active)$(focusId)?.focus({preventScroll:true});}
  },
  radarPoint(kind,x,y){if(kind===-2){radar.clearRect(0,0,240,240);radar.strokeStyle='#7ccee849';radar.lineWidth=1.5;for(const r of [35,70,103]){radar.beginPath();radar.arc(120,120,r,0,Math.PI*2);radar.stroke();}radar.beginPath();radar.moveTo(17,120);radar.lineTo(223,120);radar.moveTo(120,17);radar.lineTo(120,223);radar.stroke();return;}radar.fillStyle=kind===-1?'#ffffff':kind===2||kind===3?'#ff866a':'#a4e8ff';radar.beginPath();radar.arc(120+x*100,120+y*100,kind===-1?5:3.5,0,Math.PI*2);radar.fill();}
};
touch=new TouchControls({unlock,toggleAuto:()=>action(3),nodes:{
  game:$('game'),root:$('touch-controls'),left:$('touch-move'),right:$('touch-aim'),
  leftNub:$('touch-move-nub'),rightNub:$('touch-aim-nub'),orbitLeft:$('touch-orbit-left'),
  orbitRight:$('touch-orbit-right'),auto:$('touch-auto'),toggle:$('touch-toggle'),
}},window);
const controllers=new ControllerSetup({pause,nodes:{
  open:$('controller-setup'),dialog:$('controller-dialog'),close:$('controller-close'),device:$('controller-device'),
  begin:$('controller-begin'),back:$('controller-back'),skip:$('controller-skip'),save:$('controller-save'),reset:$('controller-reset'),
  deadzone:$('controller-deadzone'),deadzoneValue:$('controller-deadzone-value'),prompt:$('controller-prompt'),
  instructions:$('controller-instructions'),raw:$('controller-raw'),preview:$('controller-preview'),
}},window);
const vr=new TabletopVR({
  graphicsControls:{group:$('vr-graphics'),quality:$('vr-quality'),colors:$('vr-colors')},
  setGraphics(values){Object.assign(saved,values);persist();$('vr-quality').value=saved.vrQuality;$('vr-colors').value=saved.vrColors;},
  button:$('enter-vr'),ready:()=>ready,module:()=>Module,state:()=>state,settings:()=>saved,
  unlock,action,pause,toggleMusic,changeSector,
  setAudio(key,value){saved[key]=value;$(key).value=value;text(`${key}-output`,`${value}%`);action({music:8,effects:9,master:7}[key],value);},
  onEntering(){touch.update(false);pause();keys.clear();cancelAnimationFrame(raf);$('controls').close();$('controller-dialog').close();$('fullscreen-help').close();text('vr-status','Starting VR…');},
  onEntered(){ $('game').classList.add('immersive');text('vr-status','Playing in VR');},
  onLeft(){ $('game').classList.remove('immersive');keys.clear();previous=[];lastTime=0;lastScreen=-1;focused=true;
    resize();cancelAnimationFrame(raf);if(ready)raf=requestAnimationFrame(frame);text('vr-status','');},
  feedback(message){text('vr-status',message);},
},window);
window.addEventListener('pagehide',()=>vr.exit());
function resize(resetTouch=true){if(resetTouch)touch?.reset();if(!ready||vr.active)return;const rect=$('canvas').getBoundingClientRect();const dpr=Math.min(window.devicePixelRatio||1,2);const scale=Math.min(dpr,2560/Math.max(rect.width,rect.height));const w=Math.max(1,Math.round(rect.width*scale)),h=Math.max(1,Math.round(rect.height*scale));if($('canvas').width!==w||$('canvas').height!==h){touch?.reset();$('canvas').width=w;$('canvas').height=h;}}
window.addEventListener('resize',resize);
const audioKeys=['music','effects','master'];
function selectAudio(row){pauseRow=(row+3)%3;audioKeys.forEach((key,i)=>$(key).parentElement.classList.toggle('selected',i===pauseRow));}
function adjustAudio(direction){const key=audioKeys[pauseRow];$(key).value=Math.max(0,Math.min(100,Number($(key).value)+direction*5));$(key).dispatchEvent(new Event('input'));}
function frame(time){if(!ready||vr.active)return;
  // iOS can settle its standalone viewport after the window resize event.
  // Match the actual display rectangle before drawing, even without an event.
  resize(false);
  const dt=lastTime?Math.min((time-lastTime)/1000,.05):0;lastTime=time;updateAudioStatus();
  let pads=[];try{pads=Array.from(navigator.getGamepads?.()||[]).filter(Boolean);}catch{}
  const pad=controllers.poll(pads,time);
  if(padIndex!==null&&(!pad||pad.index!==padIndex)){pause('Controller disconnected or changed. Resume when ready.');previous=[];}
  if(padIndex!==(pad?.index??null)){padIndex=pad?.index??null;previous=[];}
  if(controllerRevision!==controllers.revision){controllerRevision=controllers.revision;previous=[];}
  const profile=controllers.profile(pad),mapped=readPad(pad,profile);
  text('connection',pad?(mapped?`${profile?'Saved controller setup':'Gamepad connected'} · Start / Pause to launch`:'Controller detected · Choose Controller setup to enable its controls'):
    vr.supported?'Quest / XR controllers: Enter VR. Screen mode needs a gamepad or keyboard.':touch.enabled?'Touch ready · tap Launch. Landscape gives more room.':'Connect a gamepad and press any button');
  const input=controllers.active||controllers.quiet?null:mapped,press=input?edges(input.buttons,previous):[];previous=input?.buttons||[];
  if(document.visibilityState==='visible'&&focused&&!$('controls').open&&!$('fullscreen-help').open&&!controllers.active){
    if(press.some(Boolean))unlock();
    if(press[9]||press[0]){if(state.screen===1){if(press[9])action(1);}else action(0);}
    if(press[1]&&(state.screen===2||state.screen===3))action(2);
    if(press[3])action(3);if(press[2])toggleMusic();if(press[11]&&state.screen===0)action(5);
    if(state.screen===0){if(press[14])changeSector(-1);if(press[15])changeSector(1);}
    if(state.screen===2){if(press[12])selectAudio(pauseRow-1);if(press[13])selectAudio(pauseRow+1);if(press[14])adjustAudio(-1);if(press[15])adjustAudio(1);}
  }
  const axis=(positive,negative)=>Number(keys.has(positive))-Number(keys.has(negative));
  touch.update(ready&&!controllers.active&&!$('controls').open&&!$('fullscreen-help').open&&Module._web_stat(0)===1&&!state.dying,saved.autoFire);
  const finger=touch.read();
  const mx=finger.moveActive?finger.mx:input?.mx||axis('KeyD','KeyA'),my=finger.moveActive?finger.my:input?.my||axis('KeyS','KeyW');
  const ax=finger.aimActive?finger.ax:input?.ax||axis('ArrowRight','ArrowLeft'),ay=finger.aimActive?finger.ay:input?.ay||axis('ArrowDown','ArrowUp');
  Module._web_frame(document.hidden?0:dt,mx,my,ax,ay,finger.orbitActive?finger.orbit:input?.orbit||axis('KeyE','KeyQ'),+(finger.fire||!!input?.fire||keys.has('Space')),+(!!input?.focus||keys.has('ShiftLeft')), $('canvas').width,$('canvas').height);
  raf=requestAnimationFrame(frame);
}
const handled=['KeyW','KeyA','KeyS','KeyD','ArrowLeft','ArrowRight','ArrowUp','ArrowDown','Space','KeyQ','KeyE','ShiftLeft','Enter','Escape','KeyP','KeyF','KeyM','KeyC','KeyB'];
window.addEventListener('keydown',e=>{if(vr.active||controllers.active)return;unlock();if($('controls').open||$('fullscreen-help').open)return;if(!handled.includes(e.code))return;
  // Preserve button activation, tab navigation, and native range interaction in menus.
  if((e.target.tagName==='BUTTON'&&['Space','Enter'].includes(e.code))||(e.target.tagName==='INPUT'&&state.screen!==1))return;
  e.preventDefault();keys.add(e.code);if(e.repeat||!ready)return;
  if(e.code==='Enter')action(0);if(['Escape','KeyP'].includes(e.code))action(1);if(e.code==='KeyB')action(2);if(e.code==='KeyF')action(3);if(e.code==='KeyM')toggleMusic();if(e.code==='KeyC')action(5);
  if(state.screen===0&&e.code==='ArrowLeft')changeSector(-1);if(state.screen===0&&e.code==='ArrowRight')changeSector(1);
  if(state.screen===2){if(e.code==='ArrowUp')selectAudio(pauseRow-1);if(e.code==='ArrowDown')selectAudio(pauseRow+1);if(e.code==='ArrowLeft')adjustAudio(-1);if(e.code==='ArrowRight')adjustAudio(1);}
});
window.addEventListener('keyup',e=>keys.delete(e.code));window.addEventListener('blur',()=>{focused=false;if(!vr.active)pause('Window focus lost. Resume when ready.');});window.addEventListener('focus',()=>{focused=true;});document.addEventListener('visibilitychange',()=>{lastTime=0;if(document.hidden&&!vr.active)pause('Game paused while you were away.');});for(const event of ['pointerdown','pointerup','touchend','click'])document.addEventListener(event,unlock,{capture:true,passive:true});
$('canvas').addEventListener('webglcontextlost',e=>{e.preventDefault();fail('Graphics context was lost.');});
for(const id of ['launch','retry','resume'])$(id).onclick=()=>action(0);$('pause').onclick=()=>pause();for(const id of ['return-title','end-title'])$(id).onclick=()=>action(2);
$('sector-prev').onclick=()=>changeSector(-1);$('sector-next').onclick=()=>changeSector(1);$('auto-toggle').onclick=()=>action(3);$('cadence-toggle').onclick=()=>action(5);$('sound-toggle').onpointerdown=()=>{musicActivationGesture=!saved.muted&&!audioWasRunning;};
$('sound-toggle').onkeydown=e=>{if(['Enter','Space'].includes(e.code))musicActivationGesture=!saved.muted&&!audioRunning();};
$('sound-toggle').onclick=()=>{
  // Pointer-down can resume audio several frames before click; retain the original intent.
  if(musicActivationGesture){musicActivationGesture=false;unlock();}else toggleMusic();
};
for(const [key,id]of [['master',7],['music',8],['effects',9]])$(key).oninput=()=>{saved[key]=Number($(key).value);text(`${key}-output`,`${saved[key]}%`);action(id,saved[key]);};
$('audio-unlock').onclick=unlock;
protectGameSelection($('game'),()=>ready&&!vr.active&&Module._web_stat(0)===1,window);
setupFullscreen({game:$('game'),button:$('fullscreen'),showHelp(){pause();if(!$('fullscreen-help').open)$('fullscreen-help').showModal();}},window);
$('close-fullscreen-help').onclick=()=>$('fullscreen-help').close();
$('help').onclick=()=>{pause();$('controls').showModal();};$('close-help').onclick=()=>$('controls').close();
if(!navigator.getGamepads)text('connection','Gamepad API unavailable · Keyboard controls are ready');
if(document.modelContext?.registerTool){try{Promise.resolve(document.modelContext.registerTool({name:'pause_domeblast',description:'Pause the active Domeblast run without losing progress.',inputSchema:{type:'object',properties:{},additionalProperties:false},annotations:{readOnlyHint:false},execute(input){if(!input||typeof input!=='object'||Object.keys(input).length)throw Error('Expected an empty object.');if(!ready)throw Error('Game is still loading.');pause();return {screen:Module._web_stat(0)};}})).catch(()=>{});}catch{}}
const engine=document.createElement('script');engine.src='./domeblast.js';engine.onerror=()=>fail('The game download failed.');document.body.append(engine);
