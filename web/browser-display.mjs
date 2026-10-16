// Safari can select HUD text even when the gesture starts over the canvas.
export function protectGameSelection(game,isPlaying,env=globalThis){
  const editable=target=>target?.closest?.('input,textarea,select,[contenteditable="true"]');
  const clear=()=>{
    if(!isPlaying())return;
    const selection=env.getSelection?.();
    if(selection&&!selection.isCollapsed&&!editable(selection.anchorNode?.parentElement)&&
      (game.contains(selection.anchorNode)||game.contains(selection.focusNode)))selection.removeAllRanges();
  };
  for(const type of ['selectstart','contextmenu'])game.addEventListener(type,event=>{
    if(isPlaying()&&!editable(event.target)){event.preventDefault();clear();}
  });
  game.addEventListener('pointerdown',clear,{capture:true,passive:true});
  env.document.addEventListener('selectionchange',clear);
}

export function setupFullscreen({game,button,showHelp},env=globalThis){
  const doc=env.document;
  const standalone=env.matchMedia?.('(display-mode: standalone)');
  const fullscreen=env.matchMedia?.('(display-mode: fullscreen)');
  const installed=()=>!!(env.navigator?.standalone||standalone?.matches||fullscreen?.matches);
  const active=()=>!!(doc.fullscreenElement||doc.webkitFullscreenElement);
  const available=()=>!!((game.requestFullscreen&&doc.fullscreenEnabled!==false)||
    (game.webkitRequestFullscreen&&doc.webkitFullscreenEnabled!==false));
  const update=()=>{
    button.hidden=installed()&&!active();
    button.setAttribute('aria-label',active()?'Exit fullscreen':available()?'Enter fullscreen':'Show instructions to play without browser bars');
    button.setAttribute('aria-pressed',String(active()));
  };
  button.onclick=async()=>{
    if(button.disabled)return;
    button.disabled=true;
    try{
      if(active()){
        const exit=doc.exitFullscreen||doc.webkitExitFullscreen;
        if(!exit)throw Error('Fullscreen exit unavailable');
        await exit.call(doc);
      }else if(available()){
        if(game.requestFullscreen&&doc.fullscreenEnabled!==false)await game.requestFullscreen({navigationUI:'hide'});
        else await game.webkitRequestFullscreen();
      }else if(!installed())showHelp();
    }catch{showHelp();}
    finally{button.disabled=false;update();}
  };
  for(const type of ['fullscreenchange','webkitfullscreenchange'])doc.addEventListener(type,update);
  for(const mode of [standalone,fullscreen])mode?.addEventListener?.('change',update);
  update();
}
