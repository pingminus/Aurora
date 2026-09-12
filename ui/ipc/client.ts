import { parseState, type BrowserState, type Request } from './protocol.js';

declare global {
  interface Window {
    cefQuery?: (query: { request: string; persistent: false; onSuccess: (value: string) => void;
      onFailure: (code: number, message: string) => void }) => number;
    cefQueryCancel?: (id: number) => void;
  }
}

export class NativeClient {
  // Serialize mutation and polling requests so an old snapshot cannot overwrite a new one.
  private queue: Promise<unknown> = Promise.resolve();

  request(request: Request): Promise<BrowserState> {
    const result = this.queue.then(() => this.send(request));
    this.queue = result.catch(() => undefined);
    return result;
  }

  private send(request: Request): Promise<BrowserState> {
    return new Promise((resolve, reject) => {
      if (!window.cefQuery) {
        reject(new Error('Native connection unavailable. Launch Aurora to browse.'));
        return;
      }
      let id: number | undefined;
      const timeout = window.setTimeout(() => {
        if (id !== undefined) window.cefQueryCancel?.(id);
        reject(new Error('The browser is not responding. Retrying connection…'));
      }, 5000);
      try {
        id = window.cefQuery({
          request: JSON.stringify(request), persistent: false,
          onSuccess: (raw) => {
            window.clearTimeout(timeout);
            try { resolve(parseState(raw)); } catch (error) { reject(error); }
          },
          onFailure: (_code, message) => { window.clearTimeout(timeout); reject(new Error(message)); },
        });
      } catch (error) {
        window.clearTimeout(timeout);
        reject(error);
      }
    });
  }
}
