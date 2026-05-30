// 为所有代码块添加行号

const fs = require('fs');
const path = require('path');

const apiDir = path.join(__dirname, '../api');

function convertFile(filePath) {
  let content = fs.readFileSync(filePath, 'utf8');
  let modified = false;

  // 替换 ```python 为 ```python:line-numbers
  const pythonRegex = /```python\n/g;
  if (pythonRegex.test(content)) {
    content = content.replace(pythonRegex, '```python:line-numbers\n');
    modified = true;
  }

  // 替换 ```lua 为 ```lua:line-numbers
  const luaRegex = /```lua\n/g;
  if (luaRegex.test(content)) {
    content = content.replace(luaRegex, '```lua:line-numbers\n');
    modified = true;
  }

  // 替换 ```typescript 为 ```typescript:line-numbers
  const tsRegex = /```typescript\n/g;
  if (tsRegex.test(content)) {
    content = content.replace(tsRegex, '```typescript:line-numbers\n');
    modified = true;
  }

  if (modified) {
    fs.writeFileSync(filePath, content, 'utf8');
    console.log(`✓ Updated: ${path.basename(filePath)}`);
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

console.log(`\nUpdated ${count} files`);
