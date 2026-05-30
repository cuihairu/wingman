// 将 CodeTabs 组件语法转换为 VitePress 内置 code-group 语法

const fs = require('fs');
const path = require('path');

const apiDir = path.join(__dirname, '../api');

function convertFile(filePath) {
  let content = fs.readFileSync(filePath, 'utf8');
  let modified = false;

  // 替换 CodeTabs 组件为 code-group
  // 模式 1: <CodeTabs> ... </CodeTabs>
  const codeTabsRegex = /<CodeTabs>\s*(:::\s*slot\s+python[\s\S]*?):::\s*slot\s+lua([\s\S]*?):::\s*<\/CodeTabs>/g;

  content = content.replace(codeTabsRegex, (match, pythonContent, luaContent) => {
    modified = true;

    // 提取代码块
    const extractCode = (text) => {
      const codeMatch = text.match(/```(?:python|lua)\n([\s\S]*?)```/);
      return codeMatch ? codeMatch[1].trim() : '';
    };

    const pythonCode = extractCode(pythonContent);
    const luaCode = extractCode(luaContent);

    // 生成新的 code-group 语法
    return `::: code-group\n\n\`\`\`python [Python]\n${pythonCode}\n\`\`\`\n\n\`\`\`lua [Lua]\n${luaCode}\n\`\`\`\n\n:::`;
  });

  // 模式 2: 简单的 ## Python / ## Lua 分隔
  const simpleRegex = /## Python\n\n```python\n([\s\S]*?)```\n\n## Lua\n\n```lua\n([\s\S]*?)```/g;

  content = content.replace(simpleRegex, (match, pythonCode, luaCode) => {
    modified = true;
    return `::: code-group\n\n\`\`\`python [Python]\n${pythonCode.trim()}\n\`\`\`\n\n\`\`\`lua [Lua]\n${luaCode.trim()}\n\`\`\`\n\n:::`;
  });

  if (modified) {
    fs.writeFileSync(filePath, content, 'utf8');
    console.log(`✓ Converted: ${path.basename(filePath)}`);
    return true;
  }
  return false;
}

// 处理所有 API 文档
const files = fs.readdirSync(apiDir).filter(f => f.endsWith('.md'));
let count = 0;

files.forEach(file => {
  const filePath = path.join(apiDir, file);
  if (convertFile(filePath)) {
    count++;
  }
});

console.log(`\nConverted ${count} files`);
