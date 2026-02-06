# Git仓库上下文 - paper项目

## 项目概述
微架构攻击实验代码集合，包含多个安全研究项目。

## 项目结构
```
paper/
├── inception/          # 幽灵/熔断漏洞研究
├── retbleed/           # Retbleed漏洞研究
├── Flush-Reload/       # 缓存侧信道攻击
├── demos/              # 自定义demo代码
├── own_test/           # 个人测试代码
├── flush+reload-split/ # Flush+Reload攻击变体
└── .gitignore          # Git忽略规则
```

## 仓库信息
- **远程仓库**: git@github.com:Phobia-Cosmos/paper.git
- **分支**: main
- **当前状态**: 代码已推送到GitHub

## Git操作命令

### 日常开发流程
```bash
# 1. 查看修改状态
git status

# 2. 添加修改的文件
git add <文件名或目录>

# 3. 提交修改
git commit -m "描述你的修改内容"

# 4. 推送到GitHub
git push origin main

# 5. 同步他人修改
git pull
```

### 查看历史
```bash
# 查看提交历史
git log --oneline -10

# 查看具体修改
git diff

# 查看某个文件的历史
git log -p <文件名>
```

## 多电脑同步指南

### PC1 → PC2 同步
```bash
# 在PC1上推送
git add .
git commit -m "更新描述"
git push
```

```bash
# 在PC2上拉取
git pull
```

### PC2有修改需要合并
```bash
# 在PC2上
git add .
git commit -m "PC2上的修改"
git push
```

```bash
# 在PC1上
git pull
# 如果有冲突，手动解决后
git add .
git commit -m "解决冲突"
git push
```

### 处理冲突
1. 运行 `git pull` 获取最新代码
2. 打开冲突文件，查看 `<<<<<<<` 和 `>>>>>>>` 标记
3. 手动保留正确的代码
4. `git add <冲突文件>`
5. `git commit` 完成合并

## Gitignore配置
已配置排除所有：
- 编译产物（.o, .ko, .so, .elf, bin等）
- 可执行文件
- 临时文件（~, .swp, .tmp等）
- 敏感配置（.claude.json, .env等）

## GitHub Token配置
如需使用GitHub CLI (gh命令)，需要在环境变量中设置：
```bash
export GITHUB_TOKEN="your_token_here"
```

或使用SSH认证（推荐）：
```bash
# 确保SSH key已添加到GitHub
ssh -T git@github.com
```

## 常用别名
```bash
# 添加到 ~/.gitconfig
[alias]
  st = status
  co = checkout
  br = branch
  ci = commit
  df = diff
  lg = log --oneline -10
```

---
创建时间: 2026-02-06
最后更新: 2026-02-06
