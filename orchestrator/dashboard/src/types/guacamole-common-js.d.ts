/**
 * guacamole-common-js 的最小类型声明（社区包未带完整 d.ts，
 * 只声明本工程用到的隧道/客户端/输入面）。
 */
declare module 'guacamole-common-js' {
  export class WebSocketTunnel {
    constructor(url: string);
    uuid: string;
    state: number;
    onopen?: () => void;
    onclose?: (status: string) => void;
    onerror?: (status: unknown) => void;
    connect(): void;
    disconnect(): void;
  }

  export interface GuacamoleMouseState {
    x: number;
    y: number;
    left: boolean;
    middle: boolean;
    right: boolean;
    up: boolean;
    down: boolean;
  }

  export class Mouse {
    constructor(element: HTMLElement);
    onmousedown?: (state: GuacamoleMouseState) => void;
    onmouseup?: (state: GuacamoleMouseState) => void;
    onmousemove?: (state: GuacamoleMouseState) => void;
  }

  export class Keyboard {
    constructor(element: HTMLElement | Document | Window);
    onkeydown?: (keysym: number) => void;
    onkeyup?: (keysym: number) => void;
  }

  export class Touchscreen {
    constructor(element: HTMLElement);
    onmousedown?: (state: { x: number; y: number }) => void;
    onmousemove?: (state: { x: number; y: number }) => void;
    onmouseup?: (state: { x: number; y: number }) => void;
  }

  export interface GuacamoleDisplay {
    getElement(): HTMLElement;
    getWidth(): number;
    getHeight(): number;
    scale: number;
    showCursor(show: boolean): void;
    onUpdate?: (dirty: boolean) => void;
    onresize?: (width: number, height: number) => void;
  }

  export class Client {
    constructor(tunnel: WebSocketTunnel);
    getDisplay(): GuacamoleDisplay;
    connect(param?: string): void;
    disconnect(): void;
    sendMouseState(state: GuacamoleMouseState): void;
    sendKeyEvent(pressed: number, keysym: number): void;
    sendSize(width: number, height: number): void;
    onerror?: (status: unknown) => void;
    onstatechange?: (state: number) => void;
  }
}
