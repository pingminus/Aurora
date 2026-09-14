import http from 'node:http';

// Deliberately public, synthetic tones only. This is test website code, never
// injected into user pages and never part of the volume-control implementation.
const page = (name, frequency) => `<!doctype html><html lang="en"><meta charset="utf-8">
<title>Audio ${name} — ${frequency} Hz</title>
<style>body{font:20px system-ui;background:#111c20;color:#eef7f5;padding:60px}button{font:inherit;padding:18px;margin:10px}p{max-width:700px}code{color:#a9dcc9}</style>
<h1>Audio tab ${name} · ${frequency} Hz</h1>
<p>This local test page produces a quiet continuous tone. Open A and B in separate tabs, start both, then use OpenGod's tab controls. The other tone must remain unchanged.</p>
<button id="start">Play ${frequency} Hz tone</button><button id="stop">Stop tone</button>
<p id="state" role="status">Stopped</p><a href="/tone-${name === 'A' ? 'b' : 'a'}">Other fixture</a>
<script>
let context, oscillator;
document.querySelector('#start').onclick=async()=>{
  if(context) await context.close();
  context=new AudioContext(); await context.resume();
  oscillator=context.createOscillator(); oscillator.frequency.value=${frequency};
  const level=context.createGain(); level.gain.value=0.08;
  oscillator.connect(level).connect(context.destination); oscillator.start();
  document.querySelector('#state').textContent='Playing ${frequency} Hz';
};
document.querySelector('#stop').onclick=async()=>{if(context)await context.close();context=null;document.querySelector('#state').textContent='Stopped';};
</script></html>`;
const server=http.createServer((request,response)=>{
  if(request.url==='/tone-a'||request.url==='/tone-b'){
    const isA=request.url==='/tone-a';
    response.writeHead(200,{'Content-Type':'text/html; charset=utf-8','Cache-Control':'no-store'});
    response.end(page(isA?'A':'B',isA?440:660));
  }else{response.writeHead(404);response.end('Use /tone-a or /tone-b');}
});
server.listen(8765,'127.0.0.1',()=>console.log('Audio fixtures: http://127.0.0.1:8765/tone-a and /tone-b'));
