/**
 * guacamole-common-js 的最小类型声明（社区包未带完整 d.ts，
 * 只声明本工程用到的隧道/客户端/输入面/流）。
 *
 * 包的真实形状是 UMD：全部构造器挂在 default 导出对象上
 * （webpack 生产构建实测 "possible exports: default"——具名 ESM
 * 导入在转译层过不掉）。因此运行时代码必须
 * `import Guacamole from 'guacamole-common-js'`；具名导出仅供
 * `import type` 类型引用（转译后擦除，不触碰运行时形状）。
 *
 * 流 API（InputStream/OutputStream/BlobWriter/ack 码）按 1.5.0
 * dist/esm 实测形状声明，服务剪贴板与文件传输（设计 §14/§15）。
 */
declare module 'guacamole-common-js' {
  export interface GuacamoleMouseState {
    x: number;
    y: number;
    left: boolean;
    middle: boolean;
    right: boolean;
    up: boolean;
    down: boolean;
  }

  export interface GuacamoleDisplay {
    getElement(): HTMLElement;
    getWidth(): number;
    getHeight(): number;
    /**
     * 等比缩放画面（1 = 原始分辨率）。1.5.0 是方法而非可写属性
     * （dist/esm 实测 this.scale = function(scale)），赋值只会顶掉
     * 方法、缩放永不生效——必须调用。
     */
    scale(scale: number): void;
    getScale(): number;
    showCursor(show: boolean): void;
    onUpdate?: (dirty: boolean) => void;
    onresize?: (width: number, height: number) => void;
  }

  /**
   * SessionRecording 隧道源的最小面（录像是 Blob 时由
   * recordingPlayer.ts 的适配器提供——1.5.0 的 Blob 直连分支有上游
   * 缺陷，见 recordingPlayer.ts 头注）。
   */
  export interface RecordingTunnelSource {
    connect(data?: string): void;
    sendMessage(elements: string[]): void;
    disconnect(): void;
    /** SessionRecording 构造后接管：逐条装载指令 */
    oninstruction?: (opcode: string, args: string[]) => void;
    onerror?: (status: { message: string }) => void;
    onstatechange?: (state: number) => void;
  }

  export const Tunnel: {
    State: { CONNECTING: 0; OPEN: 1; CLOSED: 2; UNSTABLE: 3 };
  };

  /**
   * Guacamole 协议指令解析器（纯字符串处理，无 DOM 依赖）：
   * receive 喂入指令流文本，逐条回调 oninstruction；非法流抛 Error。
   */
  export class Parser {
    oninstruction?: (opcode: string, args: string[]) => void;
    receive(text: string): void;
  }

  /**
   * 会话录像回放（设计 §16「回放」）：构造时即开始解析录像源，
   * 帧以 sync 指令毫秒时间戳收口，回放位置以首帧为原点（相对毫秒）。
   * 播放/暂停/跳转全在浏览器本地，不经网关。
   */
  export class SessionRecording {
    /** 录像源：Blob 直连（1.5.0 有缺陷，勿用）或隧道适配器 */
    constructor(source: Blob | RecordingTunnelSource);
    /** 回放画面（div 包裹的 canvas 组），由调用方挂到 DOM */
    getDisplay(): GuacamoleDisplay;
    isPlaying(): boolean;
    /** 当前回放位置（相对首帧的毫秒数） */
    getPosition(): number;
    /** 录像总时长（毫秒；解析完成后恒定） */
    getDuration(): number;
    play(): void;
    pause(): void;
    /** 跳到指定位置；播放中跳转后继续播，暂停中跳转后保持暂停 */
    seek(positionMs: number, callback?: () => void): void;
    /** 中止进行中的 seek（位置停在已到达处） */
    cancel(): void;
    /** 终止录像解析（Blob 源播放中调用即停止加载后续帧） */
    abort(): void;
    connect(data?: string): void;
    disconnect(): void;
    /** 解析完成（时长/帧就绪，可 play/seek） */
    onload?: () => void;
    /** 解析失败（非法指令流），回放不可用 */
    onerror?: (message: string) => void;
    /** 解析进度（每收口一帧触发一次）：duration 总时长，parsedSize 已解析字节 */
    onprogress?: (durationMs: number, parsedSize: number) => void;
    onplay?: () => void;
    onpause?: () => void;
    /** 位置变化（播放逐帧推进与 seek 都走这里） */
    onseek?: (positionMs: number, current: number, total: number) => void;
  }

  export class WebSocketTunnel {
    constructor(url: string);
    uuid: string;
    state: number;
    onopen?: () => void;
    onclose?: (status: GuacamoleStatus) => void;
    onerror?: (status: GuacamoleStatus) => void;
    connect(): void;
    disconnect(): void;
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

  /** 远端 → 浏览器的流（剪贴板/文件下载）：收 blob、逐块 ack */
  export class InputStream {
    index: number;
    onblob?: (data: string) => void;
    onend?: () => void;
    sendAck(message: string, code: number): void;
  }

  /** 浏览器 → 远端的流（剪贴板发送/文件上传）：发 blob、收 ack */
  export class OutputStream {
    index: number;
    onack?: (status: GuacamoleStatus) => void;
    sendBlob(data: string): void;
    sendEnd(): void;
  }

  /** Blob → base64 分块上传（1.5.0 只有 sendBlob；完成回调里手动 sendEnd） */
  export class BlobWriter {
    constructor(stream: OutputStream);
    sendBlob(blob: Blob): void;
    oncomplete?: () => void;
    /** 仅本地读文件失败触发；服务端错误 ack 不走这里（见 onack） */
    onerror?: (status: GuacamoleStatus) => void;
    /** 每个 blob 的 ack 到达时触发（blob = 正在发送的文件，offset = 已发字节） */
    onprogress?: (blob: Blob, offset: number) => void;
    /**
     * 每个 ack 都转发到这里——包括错误 ack（dist/esm 实测：错误 ack 转发后
     * 直接 return，发送循环停摆且 onerror 不触发）。上传失败判定必须走
     * status.code !== 0，否则失败时 Promise 永不落定。
     */
    onack?: (status: GuacamoleStatus) => void;
  }

  export interface GuacamoleStatus {
    code: number;
    message: string;
  }

  /**
   * 远端文件系统对象（guacd `filesystem` 指令，设计 §15 SSH/SFTP 树）：
   * 目录体与文件内容都以「name = 绝对路径」的输入流下发（目录体是 JSON
   * listing），上传用 createOutputStream（`put` 指令）。
   */
  export class GuacamoleObject {
    index: number;
    /** 请求 name（对象内路径）对应的输入流；body 到达时回调 */
    requestInputStream(
      name: string,
      bodyCallback?: (stream: InputStream, mimetype: string) => void,
    ): void;
    /** 创建对象输出流（`put`；name 为对象内路径，写在该路径上） */
    createOutputStream(mimetype: string, name: string): OutputStream;
    onbody?: (stream: InputStream, mimetype: string, name: string) => void;
    onundefine?: () => void;
  }

  export class Client {
    constructor(tunnel: WebSocketTunnel);
    getDisplay(): GuacamoleDisplay;
    connect(param?: string): void;
    disconnect(): void;
    sendMouseState(state: GuacamoleMouseState): void;
    sendKeyEvent(pressed: number, keysym: number): void;
    sendSize(width: number, height: number): void;
    /** 剪贴板写入通道（text/plain；readOnly 会话隐藏发送 UI） */
    createClipboardStream(mimetype: string): OutputStream;
    /** 文件上传通道（SSH=SFTP / RDP=驱动器重定向；VNC 无文件通道） */
    createFileStream(mimetype: string, filename: string): OutputStream;
    /** 对收到的流逐块确认（进度驱动：不 ack 远端不再发） */
    sendAck(index: number, message: string, code: number): void;
    /** 远端剪贴板更新（SSH/RDP/VNC 各按协议能力触发） */
    onclipboard?: (stream: InputStream, mimetype: string) => void;
    /** 远端文件下发（SFTP 下载 / RDP 驱动器读回） */
    onfile?: (stream: InputStream, filename: string, mimetype: string) => void;
    /** 远端文件系统对象上线（SSH=SFTP / RDP=驱动器；§15 文件浏览器入口） */
    onfilesystem?: (object: GuacamoleObject, name: string) => void;
    onerror?: (status: GuacamoleStatus) => void;
    onstatechange?: (state: number) => void;
  }

  /** ack/错误码（0x0000 成功家族，与 guacd 协议对齐；键集照 1.5.0 实测） */
  export interface Status {
    Code: Record<
      | 'SUCCESS'
      | 'UNSUPPORTED'
      | 'SERVER_ERROR'
      | 'SERVER_BUSY'
      | 'UPSTREAM_TIMEOUT'
      | 'UPSTREAM_ERROR'
      | 'RESOURCE_NOT_FOUND'
      | 'RESOURCE_CONFLICT'
      | 'RESOURCE_CLOSED'
      | 'UPSTREAM_NOT_FOUND'
      | 'UPSTREAM_UNAVAILABLE'
      | 'SESSION_CONFLICT'
      | 'SESSION_TIMEOUT'
      | 'SESSION_CLOSED'
      | 'CLIENT_BAD_REQUEST'
      | 'CLIENT_UNAUTHORIZED'
      | 'CLIENT_FORBIDDEN'
      | 'CLIENT_TIMEOUT',
      number
    >;
  }

  const Guacamole: {
    WebSocketTunnel: typeof WebSocketTunnel;
    Mouse: typeof Mouse;
    Keyboard: typeof Keyboard;
    Touchscreen: typeof Touchscreen;
    Client: typeof Client;
    BlobWriter: typeof BlobWriter;
    GuacamoleObject: typeof GuacamoleObject;
    SessionRecording: typeof SessionRecording;
    Tunnel: { State: { CONNECTING: 0; OPEN: 1; CLOSED: 2; UNSTABLE: 3 } };
    Parser: typeof Parser;
    Status: Status;
  };
  export default Guacamole;
}
