import * as vscode from 'vscode';
import { WingmanDebugger } from './debugger';
import { WingmanLanguageServer } from './languageServer';

let debugger: WingmanDebugger | undefined;
let languageServer: WingmanLanguageServer | undefined;

export function activate(context: vscode.ExtensionContext) {
  console.log('Wingman Extension is now active!');

  // 初始化语言服务器
  languageServer = new WingmanLanguageServer();
  context.subscriptions.push(languageServer);

  // 注册运行脚本命令
  const runCommand = vscode.commands.registerCommand('wingman.runScript', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
      vscode.window.showWarningMessage('请先打开一个 Lua 脚本文件');
      return;
    }

    const scriptPath = editor.document.uri.fsPath;
    await runScript(scriptPath);
  });

  // 注册调试脚本命令
  const debugCommand = vscode.commands.registerCommand('wingman.debugScript', async () => {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
      vscode.window.showWarningMessage('请先打开一个 Lua 脚本文件');
      return;
    }

    vscode.debug.startDebugging(undefined, {
      type: 'wingman',
      request: 'launch',
      name: 'Wingman: 调试 Lua 脚本',
      script: editor.document.uri.fsPath,
      noDebug: false
    });
  });

  // 注册停止脚本命令
  const stopCommand = vscode.commands.registerCommand('wingman.stopScript', async () => {
    vscode.debug.stopDebugging(undefined);
  });

  // 注册连接服务器命令
  const connectCommand = vscode.commands.registerCommand('wingman.connectServer', async () => {
    const config = vscode.workspace.getConfiguration('wingman.server');
    const host = config.get<string>('host', 'localhost');
    const port = config.get<number>('port', 8080);

    const result = await vscode.window.showInputBox({
      prompt: '输入 Wingman 服务器地址',
      value: `${host}:${port}`,
      placeHolder: 'localhost:8080'
    });

    if (result) {
      const [h, p] = result.split(':');
      await connectToServer(h, parseInt(p) || 8080);
    }
  });

  // 注册显示文档命令
  const docsCommand = vscode.commands.registerCommand('wingman.showDocumentation', () => {
    const panel = vscode.window.createWebviewPanel(
      'wingmanDocs',
      'Wingman API 文档',
      vscode.ViewColumn.One,
      { enableScripts: true }
    );

    panel.webview.html = getDocumentationHtml();
  });

  // 注册调试适配器工厂
  context.subscriptions.push(
    vscode.debug.registerDebugAdapterDescriptorFactory('wingman', {
      createDebugAdapterDescriptor: () => {
        return new vscode.DebugAdapterInlineImplementation(new WingmanDebugger());
      }
    })
  );

  context.subscriptions.push(
    runCommand,
    debugCommand,
    stopCommand,
    connectCommand,
    docsCommand
  );

  // 自动连接
  const autoConnect = vscode.workspace.getConfiguration('wingman').get<boolean>('autoConnect', true);
  if (autoConnect) {
    const config = vscode.workspace.getConfiguration('wingman.server');
    await connectToServer(
      config.get<string>('host', 'localhost'),
      config.get<number>('port', 8080)
    );
  }
}

async function runScript(scriptPath: string): Promise<void> {
  const config = vscode.workspace.getConfiguration('wingman.server');
  const host = config.get<string>('host', 'localhost');
  const port = config.get<number>('port', 8080);

  // TODO: 实现脚本运行逻辑
  vscode.window.showInformationMessage(`正在运行脚本: ${scriptPath}`);
}

async function connectToServer(host: string, port: number): Promise<void> {
  // TODO: 实现服务器连接逻辑
  vscode.window.showInformationMessage(`正在连接到 ${host}:${port}...`);
}

function getDocumentationHtml(): string {
  return `<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Wingman API 文档</title>
  <style>
    body { font-family: var(--vscode-font-family); padding: 20px; color: var(--vscode-foreground); }
    h1 { border-bottom: 1px solid var(--vscode-panel-border); padding-bottom: 10px; }
    h2 { margin-top: 30px; color: var(--vscode-textLink-foreground); }
    .api { margin: 10px 0; padding: 10px; background: var(--vscode-editor-background); border-radius: 4px; }
    .signature { font-family: monospace; color: var(--vscode-textLink-foreground); }
    .description { margin-top: 5px; color: var(--vscode-descriptionForeground); }
  </style>
</head>
<body>
  <h1>Wingman API 文档</h1>

  <h2>Screen 模块</h2>
  <div class="api">
    <div class="signature">screen.capture() -> boolean</div>
    <div class="description">截取整个屏幕</div>
  </div>
  <div class="api">
    <div class="signature">screen.getPixel(x: number, y: number) -> Color</div>
    <div class="description">获取指定位置的颜色</div>
  </div>
  <div class="api">
    <div class="signature">screen.findColor(color: number, region: Rect, tolerance: number) -> Point | nil</div>
    <div class="description">查找指定颜色的位置</div>
  </div>
  <div class="api">
    <div class="signature">screen.findImage(path: string, region: Rect, threshold: number) -> Point | nil</div>
    <div class="description">查找图像的位置</div>
  </div>

  <h2>Input 模块</h2>
  <div class="api">
    <div class="signature">input.click(x: number, y: number, button?: number)</div>
    <div class="description">点击指定位置</div>
  </div>
  <div class="api">
    <div class="signature">input.move(x: number, y: number, duration?: number)</div>
    <div class="description">移动鼠标（带平滑动画）</div>
  </div>
  <div class="api">
    <div class="signature">input.type(text: string, delay?: number)</div>
    <div class="description">输入文本</div>
  </div>

  <h2>Window 模块</h2>
  <div class="api">
    <div class="signature">window.find(title: string) -> handle | nil</div>
    <div class="description">查找窗口</div>
  </div>
  <div class="api">
    <div class="signature">window.activate(handle: number) -> boolean</div>
    <div class="description">激活窗口</div>
  </div>

  <h2>Process 模块</h2>
  <div class="api">
    <div class="signature">process.find(name: string) -> pid | nil</div>
    <div class="description">查找进程</div>
  </div>
  <div class="api">
    <div class="signature">process.start(path: string, args?: string, workingDir?: string) -> pid</div>
    <div class="description">启动进程</div>
  </div>

  <h2>Util 模块</h2>
  <div class="api">
    <div class="signature">util.sleep(ms: number)</div>
    <div class="description">睡眠指定毫秒</div>
  </div>
  <div class="api">
    <div class="signature">util.getTime() -> number</div>
    <div class="description">获取当前时间戳</div>
  </div>
</body>
</html>`;
}

export function deactivate() {
  if (languageServer) {
    languageServer.dispose();
  }
}
