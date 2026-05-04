import * as vscode from 'vscode';

/**
 * Wingman API 定义
 */
interface ApiDefinition {
  name: string;
  signature: string;
  description: string;
  module: string;
  returnType?: string;
  parameters?: { name: string; type: string; description: string }[];
}

const WINGMAN_APIS: ApiDefinition[] = [
  // Screen 模块
  { name: 'screen.getScreenWidth', signature: 'screen.getScreenWidth()', description: '获取屏幕宽度', module: 'screen', returnType: 'number' },
  { name: 'screen.getScreenHeight', signature: 'screen.getScreenHeight()', description: '获取屏幕高度', module: 'screen', returnType: 'number' },
  { name: 'screen.capture', signature: 'screen.capture()', description: '截取屏幕', module: 'screen', returnType: 'boolean' },
  { name: 'screen.getPixel', signature: 'screen.getPixel(x, y)', description: '获取像素颜色', module: 'screen', returnType: 'Color', parameters: [{ name: 'x', type: 'number', description: 'X 坐标' }, { name: 'y', type: 'number', description: 'Y 坐标' }] },
  { name: 'screen.findColor', signature: 'screen.findColor(color, x1, y1, x2, y2, tolerance)', description: '查找颜色', module: 'screen', returnType: 'boolean, number, number' },
  { name: 'screen.findColors', signature: 'screen.findColors(color, x1, y1, x2, y2, tolerance, count)', description: '查找多个颜色点', module: 'screen', returnType: 'table' },
  { name: 'screen.findImage', signature: 'screen.findImage(path, x1, y1, x2, y2, threshold)', description: '查找图像', module: 'screen', returnType: 'boolean, number, number' },

  // Input 模块
  { name: 'input.getMousePosition', signature: 'input.getMousePosition()', description: '获取鼠标位置', module: 'input', returnType: 'Point' },
  { name: 'input.click', signature: 'input.click(x, y, button)', description: '点击鼠标', module: 'input', parameters: [{ name: 'button', type: 'number', description: '0=左键, 1=中键, 2=右键' }] },
  { name: 'input.move', signature: 'input.move(x, y, duration)', description: '移动鼠标（贝塞尔曲线）', module: 'input' },
  { name: 'input.scroll', signature: 'input.scroll(x, y, delta)', description: '滚动滚轮', module: 'input' },
  { name: 'input.keyDown', signature: 'input.keyDown(key)', description: '按键按下', module: 'input' },
  { name: 'input.keyUp', signature: 'input.keyUp(key)', description: '按键释放', module: 'input' },
  { name: 'input.type', signature: 'input.type(text, delay)', description: '输入文本', module: 'input' },
  { name: 'input.isKeyDown', signature: 'input.isKeyDown(key)', description: '检测按键状态', module: 'input', returnType: 'boolean' },
  { name: 'input.randomDelay', signature: 'input.randomDelay(min, max)', description: '随机延迟', module: 'input' },

  // Window 模块
  { name: 'window.getForeground', signature: 'window.getForeground()', description: '获取前台窗口', module: 'window', returnType: 'number' },
  { name: 'window.find', signature: 'window.find(title)', description: '查找窗口', module: 'window', returnType: 'number' },
  { name: 'window.activate', signature: 'window.activate(hwnd)', description: '激活窗口', module: 'window', returnType: 'boolean' },
  { name: 'window.getTitle', signature: 'window.getTitle(hwnd)', description: '获取窗口标题', module: 'window', returnType: 'string' },
  { name: 'window.getBounds', signature: 'window.getBounds(hwnd)', description: '获取窗口位置', module: 'window', returnType: 'Rect' },
  { name: 'window.setBounds', signature: 'window.setBounds(hwnd, x, y, w, h)', description: '设置窗口位置', module: 'window', returnType: 'boolean' },
  { name: 'window.waitFor', signature: 'window.waitFor(title, timeout)', description: '等待窗口出现', module: 'window', returnType: 'number' },

  // Process 模块
  { name: 'process.find', signature: 'process.find(name)', description: '查找进程', module: 'process', returnType: 'number' },
  { name: 'process.start', signature: 'process.start(path, args)', description: '启动进程', module: 'process', returnType: 'number' },
  { name: 'process.wait', signature: 'process.wait(pid, timeout)', description: '等待进程退出', module: 'process', returnType: 'boolean' },
  { name: 'process.terminate', signature: 'process.terminate(pid)', description: '终止进程', module: 'process', returnType: 'boolean' },
  { name: 'process.exists', signature: 'process.exists(pid)', description: '检查进程存在', module: 'process', returnType: 'boolean' },
  { name: 'process.getPath', signature: 'process.getPath(pid)', description: '获取进程路径', module: 'process', returnType: 'string' },
  { name: 'process.waitFor', signature: 'process.waitFor(name, timeout)', description: '等待进程启动', module: 'process', returnType: 'number' },

  // System 模块
  { name: 'system.getCpuInfo', signature: 'system.getCpuInfo()', description: '获取 CPU 信息', module: 'system', returnType: 'table' },
  { name: 'system.getMemoryInfo', signature: 'system.getMemoryInfo()', description: '获取内存信息', module: 'system', returnType: 'table' },
  { name: 'system.getDiskInfo', signature: 'system.getDiskInfo(path)', description: '获取磁盘信息', module: 'system', returnType: 'table' },
  { name: 'system.getGpuInfo', signature: 'system.getGpuInfo()', description: '获取 GPU 信息', module: 'system', returnType: 'table' },
  { name: 'system.getOsInfo', signature: 'system.getOsInfo()', description: '获取系统信息', module: 'system', returnType: 'table' },
  { name: 'system.getNetworkAdapters', signature: 'system.getNetworkAdapters()', description: '获取网络适配器', module: 'system', returnType: 'table' },
  { name: 'system.getDisplayInfo', signature: 'system.getDisplayInfo()', description: '获取显示器信息', module: 'system', returnType: 'table' },
  { name: 'system.getUptime', signature: 'system.getUptime()', description: '获取运行时间', module: 'system', returnType: 'number' },
  { name: 'system.getDateTime', signature: 'system.getDateTime()', description: '获取日期时间', module: 'system', returnType: 'table' },
  { name: 'system.getProcessCount', signature: 'system.getProcessCount()', description: '获取进程数量', module: 'system', returnType: 'number' },
  { name: 'system.getThreadCount', signature: 'system.getThreadCount()', description: '获取线程数量', module: 'system', returnType: 'number' },

  // HTTP 模块
  { name: 'http.get', signature: 'http.get(url, headers)', description: 'HTTP GET 请求', module: 'http', returnType: 'table' },
  { name: 'http.post', signature: 'http.post(url, body, headers)', description: 'HTTP POST 请求', module: 'http', returnType: 'table' },
  { name: 'http.put', signature: 'http.put(url, body, headers)', description: 'HTTP PUT 请求', module: 'http', returnType: 'table' },
  { name: 'http.delete', signature: 'http.delete(url, headers)', description: 'HTTP DELETE 请求', module: 'http', returnType: 'table' },

  // JSON 模块
  { name: 'json.encode', signature: 'json.encode(obj)', description: '序列化 JSON', module: 'json', returnType: 'string' },
  { name: 'json.decode', signature: 'json.decode(str)', description: '解析 JSON', module: 'json', returnType: 'table' },

  // KV 模块
  { name: 'kv.set', signature: 'kv.set(key, value)', description: '设置键值', module: 'kv' },
  { name: 'kv.get', signature: 'kv.get(key)', description: '获取键值', module: 'kv', returnType: 'string' },
  { name: 'kv.del', signature: 'kv.del(key, ...)', description: '删除键', module: 'kv' },
  { name: 'kv.exists', signature: 'kv.exists(key)', description: '检查键存在', module: 'kv', returnType: 'boolean' },
  { name: 'kv.incr', signature: 'kv.incr(key)', description: '自增', module: 'kv' },
  { name: 'kv.decr', signature: 'kv.decr(key)', description: '自减', module: 'kv' },
  { name: 'kv.hset', signature: 'kv.hset(hash, field, value)', description: '设置哈希字段', module: 'kv' },
  { name: 'kv.hget', signature: 'kv.hget(hash, field)', description: '获取哈希字段', module: 'kv', returnType: 'string' },
  { name: 'kv.hgetall', signature: 'kv.hgetall(hash)', description: '获取所有哈希字段', module: 'kv', returnType: 'table' },
  { name: 'kv.lpush', signature: 'kv.lpush(list, value)', description: '左推入列表', module: 'kv' },
  { name: 'kv.lpop', signature: 'kv.lpop(list)', description: '左弹出列表', module: 'kv', returnType: 'string' },
  { name: 'kv.rpush', signature: 'kv.rpush(list, value)', description: '右推入列表', module: 'kv' },
  { name: 'kv.rpop', signature: 'kv.rpop(list)', description: '右弹出列表', module: 'kv', returnType: 'string' },

  // Util 模块
  { name: 'util.sleep', signature: 'util.sleep(ms)', description: '睡眠', module: 'util' },
  { name: 'util.getTime', signature: 'util.getTime()', description: '获取时间戳', module: 'util', returnType: 'number' },
  { name: 'util.random', signature: 'util.random(min, max)', description: '生成随机数', module: 'util', returnType: 'number' },
  { name: 'util.format', signature: 'util.format(fmt, ...)', description: '格式化字符串', module: 'util', returnType: 'string' },
];

/**
 * Wingman Language Server
 * 提供 Lua 语言的语法高亮、自动完成、诊断等功能
 */
export class WingmanLanguageServer implements vscode.Disposable {
  private diagnosticCollection = vscode.languages.createDiagnosticCollection('wingman');
  private disposables: vscode.Disposable[] = [];

  constructor() {
    this.registerProviders();
  }

  dispose() {
    this.diagnosticCollection.dispose();
    this.disposables.forEach(d => d.dispose());
  }

  private registerProviders() {
    // 注册自动完成提供器
    const completionProvider = vscode.languages.registerCompletionItemProvider(
      'lua',
      new WingmanCompletionProvider(),
      '.', ':'
    );
    this.disposables.push(completionProvider);

    // 注册悬停提示
    const hoverProvider = vscode.languages.registerHoverProvider(
      'lua',
      new WingmanHoverProvider()
    );
    this.disposables.push(hoverProvider);

    // 注册签名帮助
    const signatureProvider = vscode.languages.registerSignatureHelpProvider(
      'lua',
      new WingmanSignatureProvider(),
      '(', ','
    );
    this.disposables.push(signatureProvider);

    // 注册诊断提供器
    const diagnosticProvider = vscode.languages.registerDiagnosticProvider(
      'lua',
      new WingmanDiagnosticProvider()
    );
    this.disposables.push(diagnosticProvider);
  }
}

/**
 * 自动完成提供器
 */
class WingmanCompletionProvider implements vscode.CompletionItemProvider {
  provideCompletionItems(
    document: vscode.TextDocument,
    position: vscode.Position,
    token: vscode.CancellationToken,
    context: vscode.CompletionContext
  ): vscode.ProviderResult<vscode.CompletionItem[]> {
    const items: vscode.CompletionItem[] = [];
    const lineText = document.lineAt(position.line).text;
    const wordRange = document.getWordRangeAtPosition(position);

    // 检查是否在模块调用中 (如 screen.)
    if (lineText.includes('.')) {
      const beforeDot = lineText.substring(0, position.character);
      const parts = beforeDot.split('.');
      if (parts.length >= 2) {
        const module = parts[parts.length - 2].trim();
        return this.getModuleCompletions(module);
      }
    }

    // 返回所有 API
    for (const api of WINGMAN_APIS) {
      const item = new vscode.CompletionItem(api.name, vscode.CompletionItemKind.Function);
      item.detail = api.signature;
      item.documentation = new vscode.MarkdownString(api.description);
      item.insertText = api.name.substring(api.name.indexOf('.') + 1);
      items.push(item);
    }

    // 添加模块名称
    const modules = ['screen', 'input', 'window', 'process', 'system', 'http', 'json', 'kv', 'util'];
    for (const mod of modules) {
      const item = new vscode.CompletionItem(mod, vscode.CompletionItemKind.Module);
      item.documentation = `Wingman ${mod} 模块`;
      items.push(item);
    }

    return items;
  }

  private getModuleCompletions(module: string): vscode.CompletionItem[] {
    const items: vscode.CompletionItem[] = [];
    const apis = WINGMAN_APIS.filter(api => api.name.startsWith(module + '.'));

    for (const api of apis) {
      const item = new vscode.CompletionItem(api.name.substring(module.length + 1), vscode.CompletionItemKind.Function);
      item.detail = api.signature;
      item.documentation = new vscode.MarkdownString(api.description);
      items.push(item);
    }

    return items;
  }
}

/**
 * 悬停提示提供器
 */
class WingmanHoverProvider implements vscode.HoverProvider {
  provideHover(
    document: vscode.TextDocument,
    position: vscode.Position,
    token: vscode.CancellationToken
  ): vscode.ProviderResult<vscode.Hover> {
    const wordRange = document.getWordRangeAtPosition(position);
    if (!wordRange) return undefined;

    const word = document.getText(wordRange);
    const lineText = document.lineAt(position.line).text;

    // 检查是否是 API 调用
    const apiMatch = lineText.match(/(\w+)\.(\w+)/);
    if (apiMatch) {
      const fullName = `${apiMatch[1]}.${apiMatch[2]}`;
      const api = WINGMAN_APIS.find(a => a.name === fullName);
      if (api) {
        const markdown = new vscode.MarkdownString();
        markdown.appendMarkdown(`**${api.signature}**\n\n`);
        markdown.appendMarkdown(api.description);
        if (api.returnType) {
          markdown.appendMarkdown(`\n\n**返回:** \`${api.returnType}\``);
        }
        if (api.parameters) {
          markdown.appendMarkdown('\n\n**参数:**\n');
          for (const param of api.parameters) {
            markdown.appendMarkdown(`- \`${param.name}\` (\`${param.type}\`): ${param.description}\n`);
          }
        }
        return new vscode.Hover(markdown);
      }
    }

    return undefined;
  }
}

/**
 * 签名帮助提供器
 */
class WingmanSignatureProvider implements vscode.SignatureHelpProvider {
  provideSignatureHelp(
    document: vscode.TextDocument,
    position: vscode.Position,
    token: vscode.CancellationToken,
    context: vscode.SignatureHelpContext
  ): vscode.ProviderResult<vscode.SignatureHelp> {
    const lineText = document.lineAt(position.line).text;
    const before = lineText.substring(0, position.character);

    for (const api of WINGMAN_APIS) {
      if (before.includes(api.name + '(')) {
        const signature = new vscode.SignatureInformation(
          api.signature,
          api.description
        );

        if (api.parameters) {
          signature.parameters = api.parameters.map((param, i) => {
            const paramInfo = new vscode.ParameterInformation(
              `${param.name}: ${param.type}`,
              param.description
            );
            return paramInfo;
          });
        }

        return {
          signatures: [signature],
          activeParameter: this.countCommas(before) % (api.parameters?.length || 1),
          activeSignature: 0
        };
      }
    }

    return undefined;
  }

  private countCommas(str: string): number {
    let count = 0;
    let inParentheses = false;
    for (const char of str) {
      if (char === '(') inParentheses = true;
      if (char === ')') inParentheses = false;
      if (inParentheses && char === ',') count++;
    }
    return count;
  }
}

/**
 * 诊断提供器
 */
class WingmanDiagnosticProvider implements vscode.DiagnosticProvider {
  provideDiagnostics(
    document: vscode.TextDocument,
    token: vscode.CancellationToken
  ): vscode.ProviderResult<vscode.Diagnostic[]> {
    const diagnostics: vscode.Diagnostic[] = [];
    const text = document.getText();
    const lines = text.split('\n');

    for (let i = 0; i < lines.length; i++) {
      const line = lines[i];

      // 检查未关闭的字符串
      const singleQuoteCount = (line.match(/'/g) || []).length;
      const doubleQuoteCount = (line.match(/"/g) || []).length;
      if (singleQuoteCount % 2 !== 0 || doubleQuoteCount % 2 !== 0) {
        diagnostics.push(new vscode.Diagnostic(
          new vscode.Range(i, 0, i, line.length),
          '可能未关闭的字符串',
          vscode.DiagnosticSeverity.Warning
        ));
      }

      // 检查使用了不存在的 API
      const apiPattern = /(\w+)\.(\w+)\(/g;
      let match;
      while ((match = apiPattern.exec(line)) !== null) {
        const module = match[1];
        const func = match[2];
        const fullName = `${module}.${func}`;
        const exists = WINGMAN_APIS.some(api => api.name === fullName);
        if (!exists && ['screen', 'input', 'window', 'process', 'system', 'http', 'json', 'kv', 'util'].includes(module)) {
          diagnostics.push(new vscode.Diagnostic(
            new vscode.Range(i, match.index, i, match.index + fullName.length),
            `未知的 API: ${fullName}`,
            vscode.DiagnosticSeverity.Warning
          ));
        }
      }
    }

    return diagnostics;
  }
}
