import { parseTerminal } from '../ipc/terminal-protocol.js';
declare const Terminal: typeof import('@xterm/xterm').Terminal;
declare const FitAddon: { FitAddon: typeof import('@xterm/addon-fit').FitAddon };
const term=new Terminal({fontFamily:'Cascadia Mono, Consolas, monospace',fontSize:14,scrollback:3000,cursorBlink:false,allowProposedApi:false,screenReaderMode:true,theme:{background:'#101117',foreground:'#eeeeF5',cursor:'#b5baff',selectionBackground:'#6468a880'},linkHandler:{activate:()=>undefined}});
const fit=new FitAddon.FitAddon();term.loadAddon(fit);term.open(document.querySelector<HTMLElement>('#terminal')!);
// Output must never access the clipboard or open external links/windows.
term.parser.registerOscHandler(52,()=>true);
const stateLabel=document.querySelector<HTMLElement>('#terminal-state')!;
const errorLabel=document.querySelector<HTMLElement>('#terminal-error')!;
const restart=document.querySelector<HTMLButtonElement>('#restart')!;
let session='',cursor='0',status='starting',disposed=false,busy=false,inputQueue:string[]=[],queuedBytes=0;
let dimensions: {columns:number;rows:number}|undefined;
let chain: Promise<unknown>=Promise.resolve();
function error(message:string):void{errorLabel.textContent=message;errorLabel.hidden=false;}
function request(command:string,fields:Record<string,string|number>={}):Promise<string>{
 const result=chain.then(()=>new Promise<string>((resolve,reject)=>{
  if(disposed||!window.cefQuery){reject(new Error('Terminal bridge unavailable'));return;}
  let id:number|undefined;const timer=window.setTimeout(()=>{if(id!==undefined)window.cefQueryCancel?.(id);reject(new Error('Terminal request timed out. Input was not retried; check the shell before continuing.'));},5000);
  try{id=window.cefQuery({request:JSON.stringify({version:1,command,...(command==='terminalHello'?{}:{session}),...fields}),persistent:false,onSuccess:raw=>{clearTimeout(timer);resolve(raw);},onFailure:(_code,message)=>{clearTimeout(timer);reject(new Error(message));}});}catch(e){clearTimeout(timer);reject(e);}
 }));chain=result.catch(()=>undefined);return result;
}
async function clipboard(copy:boolean):Promise<void>{try{
 if(copy){const data=term.getSelection();if(data)await request('terminalCopy',{data});}
 else{const raw:unknown=JSON.parse(await request('terminalPaste'));if(!raw||typeof raw!=='object'||typeof (raw as {text?:unknown}).text!=='string')throw new Error('Invalid clipboard data');const text=(raw as {text:string}).text;if(/[\r\n]/.test(text)&&!window.confirm('Paste multiple lines into the shell? They may execute commands.'))return;term.paste(text);}
 term.focus();
 }catch(e){error(e instanceof Error?e.message:'Clipboard failed');}}
document.querySelector('#copy')!.addEventListener('click',()=>void clipboard(true));document.querySelector('#paste')!.addEventListener('click',()=>void clipboard(false));
term.attachCustomKeyEventHandler(event=>{if(event.type==='keydown'&&event.ctrlKey&&event.shiftKey&&(event.code==='KeyC'||event.code==='KeyV')){event.preventDefault();event.stopPropagation();void clipboard(event.code==='KeyC');return false;}return true;});
term.onData(data=>{if(status!=='running')return;const bytes=new TextEncoder().encode(data).length;if(bytes>65536-queuedBytes){error('Input buffer full. Wait for the shell before typing again.');return;}queuedBytes+=bytes;let chunk='';for(const point of data){chunk+=point;if(chunk.length>=2048){inputQueue.push(chunk);chunk='';}}if(chunk)inputQueue.push(chunk);});
function resize():void{if(document.hidden)return;fit.fit();dimensions={columns:Math.max(2,Math.min(400,term.cols)),rows:Math.max(1,Math.min(200,term.rows))};}
const observer=new ResizeObserver(resize);observer.observe(document.querySelector('#terminal')!);document.addEventListener('visibilitychange',()=>{if(!document.hidden){resize();term.focus();}});
async function snapshot(command:string):Promise<void>{const data=parseTerminal(await request(command,command==='terminalRead'?{cursor}:{}));
 if(session&&data.session!==session){term.reset();cursor='0';inputQueue=[];queuedBytes=0;}
 session=data.session;status=data.status;stateLabel.textContent=(data.shell||'Shell')+' · '+status;restart.disabled=status!=='exited'&&status!=='error';
 if(status!=='running'){inputQueue=[];queuedBytes=0;}
 if(data.error)error(data.error);else if(status==='exited'||status==='stopping')errorLabel.hidden=true;
 if(data.data){const bytes=Uint8Array.from(atob(data.data),c=>c.charCodeAt(0));await new Promise<void>(resolve=>term.write(bytes,resolve));}
 cursor=data.cursor;
}
restart.addEventListener('click',()=>{if(busy)return;busy=true;void snapshot('terminalRestart').then(()=>{errorLabel.hidden=true;resize();term.focus();}).catch(e=>error(String(e))).finally(()=>{busy=false;});});
async function pump():Promise<void>{if(disposed)return;if(!busy){busy=true;try{
 if(!session){await snapshot('terminalHello');resize();term.focus();}
 else{
 await snapshot('terminalRead');
 if(dimensions&&status==='running'){const next=dimensions;dimensions=undefined;await request('terminalResize',next);}
 for(let i=0;i<8&&status==='running'&&inputQueue.length;i++){const data=inputQueue.shift()!;queuedBytes-=new TextEncoder().encode(data).length;await request('terminalInput',{data});}
 await snapshot('terminalRead');
 }
 }catch(e){inputQueue=[];queuedBytes=0;error(e instanceof Error?e.message:'Terminal failed');}finally{busy=false;}}
 window.setTimeout(()=>void pump(),50);}
window.addEventListener('pagehide',()=>{disposed=true;observer.disconnect();term.dispose();});
void pump();
