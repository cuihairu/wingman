// 将 code-group 语法转换为 vitepress-plugin-tabs 语法

const fs = require('fs');
const path = require('path');

const apiDir = path.join(__dirname, '../api');

function convertFile(filePath) {
  let content = fs.readFileSync(filePath, 'utf8');
  let modified = false;

  // 替换 code-group 为 tabs
  // 模式: ::: code-group ... ```python [Python] ... ```lua [Lua] ... :::
  const codeGroupRegex = /::: code-group\n\n```python \[Python\]([\s\S]*?)```\n\n```lua \[Lua\]([\s\S]*?)```\n\n:::/g;

  content = content.replace(codeGroupRegex, (match, pythonCode, luaCode) => {
    modified = true;
    return `:::tabs\n\n== Python\n\n\`\`\`python\n${pythonCode.trim()}\n\`\`\`\n\n== Lua\n\n\`\`\`lua\n${luaCode.trim()}\n\`\`\`\n\n:::`;
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
