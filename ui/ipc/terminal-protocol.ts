export interface TerminalSnapshot { version: 1; session: string; cursor: string; status: 'starting'|'running'|'stopping'|'exited'|'error'; shell: string; error: string; data: string; }
export function parseTerminal(raw: string): TerminalSnapshot {
 if(raw.length>100000)throw new Error('Terminal response too large');
 const v: unknown=JSON.parse(raw);if(!v||typeof v!=='object'||Array.isArray(v))throw new Error('Invalid terminal response');
 const d=v as Record<string,unknown>;
 if(d.version!==1||typeof d.session!=='string'||!/^\d{1,20}$/.test(d.session)||typeof d.cursor!=='string'||!/^\d{1,20}$/.test(d.cursor)||!['starting','running','stopping','exited','error'].includes(String(d.status))||typeof d.shell!=='string'||d.shell.length>100||typeof d.error!=='string'||d.error.length>500||typeof d.data!=='string'||d.data.length>44000||!/^([A-Za-z0-9+/]{4})*([A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(d.data))throw new Error('Invalid terminal snapshot');
 return d as unknown as TerminalSnapshot;
}
