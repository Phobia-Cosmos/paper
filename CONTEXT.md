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
├── CONTEXT.md          # 本文档
└── .gitignore          # Git忽略规则
```

## 仓库信息
- **远程仓库**: git@github.com:Phobia-Cosmos/paper.git
- **当前分支**: master
- **当前状态**: 代码已推送到GitHub

## master vs main 分支名称区别

**本质上没有区别**，都是默认分支名称：

| 特性 | master | main |
|------|--------|------|
| 含义 | 传统命名 | 2020年后GitHub默认 |
| 权限 | 相同 | 相同 |
| 功能 | 完全相同 | 完全相同 |
| 历史 | Git传统默认 | 更具包容性的命名 |

**结论：** 保持一致即可，无需修改。当前使用 `master`。

## AMD与Intel架构分支策略

由于是**微架构攻击研究**，AMD和Intel在以下方面差异很大：
- 分支预测器设计
- 缓存结构
- 投机执行机制
- 特定指令行为

### 推荐分支结构
```bash
# 创建架构分支
git branch main                 # 公共代码、工具、文档
git branch amd-zen              # AMD Zen架构特定代码
git branch intel-skylake        # Intel Skylake及以后
git branch intel-haswell        # Intel Haswell等旧架构

# 开发时切换分支
git checkout amd-zen            # 在AMD机器上开发
git checkout intel-skylake      # 在Intel机器上开发

# 合并公共代码到架构分支
git checkout amd-zen
git merge main                  # 合并公共代码
```

### 快速创建所有分支
```bash
# 在任意电脑上执行一次
git checkout -b amd-zen
git push origin amd-zen

git checkout -b intel-skylake
git push origin intel-skylake

git checkout -b intel-haswell
git push origin intel-haswell
```

## Git操作命令

### 日常开发流程
```bash
# 1. 查看修改状态
git status

# 2. 添加修改的文件
git add <文件名或目录>
git add .                       # 添加所有修改

# 3. 提交修改
git commit -m "描述你的修改内容"

# 4. 推送到GitHub
git push origin master          # 推送到master分支
git push origin amd-zen         # 推送到AMD分支

# 5. 同步最新代码
git pull                        # 拉取并合并
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

## PC2同步操作指南（仅inception和retbleed目录）

### 场景描述
PC2上只有 `inception/` 和 `retbleed/` 两个目录，代码可能与PC1有差异。

### PC2首次同步步骤

```bash
# 1. 进入项目目录
cd ~/paper                       # 或你的项目路径

# 2. 克隆完整仓库
git clone git@github.com:Phobia-Cosmos/paper.git
cd paper

# 3. 查看当前状态（此时应该看到PC1的所有文件）
ls -la

# 4. 备份PC2上的原始文件（重要！）
mkdir -p ~/backup-pc2
cp -r inception/ ~/backup-pc2/
cp -r retbleed/ ~/backup-pc2/

# 5. 对比PC2的inception和retbleed
diff -r ~/backup-pc2/inception inception/
diff -r ~/backup-pc2/retbleed retbleed/
```

### PC2同步修改到GitHub

```bash
# 方法1：直接覆盖PC1的版本（如果PC2是最新）
git add inception retbleed
git commit -m "sync from PC2 - 更新inception和retbleed"
git push origin master
```

```bash
# 方法2：将PC2修改合并到PC1（如果两边都有修改）
# 先查看差异
git diff inception/ retbleed/

# 创建合并提交
git add inception retbleed
git commit -m "merge from PC2 - 合并PC2的修改"
git push origin master
```

### PC1同步PC2的修改

```bash
# 在PC1上
git pull

# 如果有冲突，手动解决：
# 1. 查看冲突文件
git status

# 2. 编辑冲突文件（包含 <<<<<<< 和 >>>>>>> 标记）
# 3. 标记已解决
git add <冲突文件>

# 4. 完成合并
git commit -m "解决与PC2的冲突"
git push
```

### PC2同步PC1的修改

```bash
# 在PC2上
git pull

# 如果PC2的inception/retbleed有本地修改，会冲突
# 查看冲突
git status

# 解决冲突后
git add inception retbleed
git commit -m "解决与PC1的冲突"
git push
```

## 处理冲突的详细步骤

1. **运行 `git pull` 获取最新代码**
   ```bash
   git pull origin master
   ```

2. **查看冲突文件**
   ```bash
   git status                          # 红色为冲突文件
   git diff --name-only --diff-filter=U  # 只显示冲突文件
   ```

3. **手动编辑冲突文件**
   ```bash
   # 打开冲突文件，看到类似：
   <<<<<<< HEAD
   PC1的代码
   =======
   PC2的代码
   >>>>>>> commit-id

   # 保留正确的代码，删除标记
   ```

4. **标记冲突已解决**
   ```bash
   git add <冲突文件>
   git add .                           # 或添加所有
   ```

5. **完成合并**
   ```bash
   git commit -m "解决冲突：描述如何解决"
   git push
   ```

## Gitignore配置
已配置排除所有：
- 编译产物（.o, .ko, .so, .elf, bin等）
- 可执行文件（无扩展名）
- 临时文件（~, .swp, .tmp等）
- 敏感配置（.claude.json, .env等）
- 日志文件

## GitHub认证

### SSH认证（推荐）
```bash
# 测试SSH连接
ssh -T git@github.com

# 如果未认证，需要将 ~/.ssh/id_rsa.pub 内容添加到：
# https://github.com/settings/keys
```

### Token认证
```bash
# 设置环境变量
export GITHUB_TOKEN="ghp_xxxxxxxxxxxxxxxxxxxxxxxx"

# 或使用GitHub CLI
gh auth login
```

## 常用别名
```bash
# 添加到 ~/.gitconfig（已全局配置）
[alias]
  st = status
  co = checkout
  br = branch
  ci = commit
  df = diff
  lg = log --oneline -10
  pl = pull
  ps = push
  save = !git add . && git commit -m "update" && git push
```

### 一键clone代码（不含git历史）

已配置全局alias `clone-clean`，用于快速clone代码并删除.git：

```bash
# 基本用法 - clone到同名目录
git clone-clean git@github.com:comsec-group/inception.git

# 指定目标目录名
git clone-clean git@github.com:comsec-group/inception.git my-inception

# 克隆其他仓库
git clone-clean git@github.com:comsec-group/retbleed.git retbleed-latest
```

**工作流程示例：**
```bash
# 在paper目录下
cd /home/undefined/cls/paper

# 克隆上游代码（不含git历史）
git clone-clean git@github.com:comsec-group/inception.git inception/upstream
git clone-clean git@github.com:comsec-group/retbleed.git retbleed/upstream

# 现在可以自由修改，不会与上游git冲突
ls -la inception/upstream/
ls -la retbleed/upstream/

# 添加到你的paper仓库
git add inception/upstream retbleed/upstream
git commit -m "add upstream inception and retbleed"
git push origin master
```

**注意：** 此方式不保留上游的git历史，适合只使用代码不跟踪上游更新的场景。

## 快速参考卡

| 操作 | 命令 |
|------|------|
| 克隆仓库 | `git clone git@github.com:Phobia-Cosmos/paper.git` |
| 查看状态 | `git status` |
| 添加修改 | `git add .` |
| 提交 | `git commit -m "message"` |
| 推送 | `git push origin master` |
| 拉取 | `git pull` |
| 创建分支 | `git checkout -b branch-name` |
| 切换分支 | `git checkout branch-name` |
| 合并分支 | `git merge branch-name` |
| 查看分支 | `git branch -a` |
| 查看历史 | `git log --oneline -5` |

---
创建时间: 2026-02-06
最后更新: 2026-02-06
