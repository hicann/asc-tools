# Pre-commit 规范引入与 PR 提交流程

本文记录本次 pre-commit 规范化改动的 PR 描述，以及从本地修改、检查、提交到推送个人 Fork 的完整流程。

## PR 标题建议

```text
ci: 引入 pre-commit 基础检查并完成全仓格式规范化
```

## PR 描述

下面的内容可直接填写到 GitCode PR 描述中；如果有对应 Issue，请补充实际链接或编号。

```markdown
## 描述

本 PR 为 asc-tools 引入并完善 pre-commit 提交检查，用于在代码提交前统一执行基础格式和合规性校验。

主要改动如下：

1. 引入 pre-commit-hooks v4.6.0：
   - 清理行尾空格，Markdown 文件除外；
   - 修复文件末尾换行，Markdown 文件除外；
   - 检查 YAML、JSON 语法；
   - 检查大文件、Git 冲突标记和私钥误提交。
2. 调整 clang-format 范围：
   - 仅格式化 examples/ 和 tests/ut/ 下的 C/C++/ASC 文件；
   - 排除 third_party、cmake/third_party 和 tests/third_party 目录；
   - 继续使用仓库根目录的 .clang-format，并通过 -i 自动修复。
3. 将 OAT hook 设置为串行执行，避免多个检查进程共用报告目录时互相覆盖结果。
4. 修复 PR 变更文件的 OAT 合规问题：
   - 为 13 个模板脚本补充 CANN License Header；
   - 精确排除根目录许可证正文 `LICENSE`，避免将许可证文件误判为缺少许可证头。
5. 对全仓执行基础格式规范化，共清理 150 个历史文件中的行尾空格和文件末尾换行，不涉及业务逻辑修改。

CI 涉及的 152 个 PR 文件已通过全部 pre-commit 检查；`LICENSE` 被精确排除后，OAT 实际检查并通过 151 个文件。对仓库执行 `--all-files` 时仍可发现 20 项未被本 PR 修改的历史问题，包括 2 个二进制文件类型问题和 18 个许可证头问题，后续需通过独立改动完成治理。

## 关联的Issue

暂无。如有对应 Issue，请填写：关联 Issue #编号

## 测试

- CI 相同的 152 个 PR 文件检查：全部通过，OAT 实际检查并通过 151 个文件；
- 行尾空格、文件末尾换行、YAML、JSON、大文件、冲突标记、私钥和 clang-format 检查：通过；
- 10 个 Python 模板文件语法检查：通过；
- 3 个 Shell 模板文件 `bash -n` 语法检查：通过；
- `pre-commit run --all-files`：OAT 检查未通过，发现 20 项未被本 PR 修改的历史合规问题；
- GitCode 远端 Git Hooks：通过；
- 格式清理忽略行尾空白后仅包含空白行删除和 OAT 串行配置，不包含业务逻辑改动。

## 文档更新

- 新增 `PRE_COMMIT_WORKFLOW.md`，记录 pre-commit 使用方法、提交命令和 PR 描述模板。

## 类型标签

- [ ] 🐛 fix: Bug 修复
- [ ] ✨ feat: 新功能
- [ ] ⚡ perf: 性能优化
- [ ] ♻️ refactor: 代码重构
- [ ] 🧪 test: 新增或修改测试
- [x] 📝 docs: 文档更新
- [x] 🔧 ci: CI/CD 配置修改
- [ ] ↩️ revert: 回退
- [x] 🧹 chore: 全仓历史格式清理
```

## 从修改到推送的 Linux 命令

### 1. 进入仓库并确认远程地址

```bash
cd /data/zzz/asc-tools

git status
git remote -v
```

本仓库的远程关系应为：

```text
origin -> https://gitcode.com/cann/asc-tools.git
fork   -> https://gitcode.com/jcmrn0930/asc-tools.git
```

如果还没有个人 Fork 远程地址，可执行：

```bash
git remote add fork https://jcmrn0930@gitcode.com/jcmrn0930/asc-tools.git
```

如果 `fork` 已存在但地址错误，可执行：

```bash
git remote set-url fork https://jcmrn0930@gitcode.com/jcmrn0930/asc-tools.git
```

### 2. 从原仓库最新 master 创建开发分支

新建分支时执行：

```bash
git fetch origin
git switch -c pre-commit-standardization origin/master
```

如果分支已经存在，直接切换：

```bash
git switch pre-commit-standardization
```

### 3. 安装并启用 pre-commit

```bash
python3 -m pip install --user "pre-commit>=4.0.0"
python3 -m pip install --user "oat-py>=1.0.1"

pre-commit --version
pre-commit install
```

`pre-commit install` 会安装 Git commit hook，之后执行 `git commit` 时会自动检查暂存文件。

### 4. 修改配置或代码

使用编辑器完成修改后查看差异：

```bash
git status
git diff
```

### 5. 对整个仓库运行检查

```bash
pre-commit run --all-files
```

行尾空格、文件末尾换行和 clang-format hook 可能自动修改文件。第一次执行因此返回失败是正常现象，需要查看改动后再次运行：

```bash
git status
git diff --check
pre-commit run --all-files
```

仅检查当前分支相对于原仓库 `master` 的改动，可以执行：

```bash
pre-commit run --from-ref origin/master --to-ref HEAD
```

### 6. 暂存文件并检查本次提交

```bash
git add -A
pre-commit run
```

如果 hook 又自动修改了文件，需要重新暂存并复查：

```bash
git add -A
pre-commit run
```

### 7. 创建提交

引入检查配置：

```bash
git commit -m "pre-commit: enable basic checks for asc-tools"
```

一次性全仓格式清理建议使用独立提交：

```bash
git commit -m "style: apply pre-commit formatting"
```

提交后确认记录和工作区状态：

```bash
git log -3 --oneline
git status
```

### 8. 推送到个人 Fork

首次推送并设置跟踪分支：

```bash
git push -u fork pre-commit-standardization
```

后续继续推送同一分支：

```bash
git push
```

HTTPS 认证时，用户名填写 `jcmrn0930`，密码位置填写 GitCode 访问令牌，不要将令牌写入命令、仓库文件或 PR 描述。

### 9. 创建 PR

推送完成后，打开以下地址创建 PR：

```text
https://gitcode.com/jcmrn0930/asc-tools/merge_requests/new?source_branch=pre-commit-standardization
```

确认：

- 源仓库：`jcmrn0930/asc-tools`；
- 源分支：`pre-commit-standardization`；
- 目标仓库：`cann/asc-tools`；
- 目标分支：`master`。

然后填写本文提供的 PR 标题和描述。

## 本次 OAT 存量问题的特殊处理

正常提交不应跳过 OAT。当前仓库在引入规范前已经存在 OAT 问题，全仓格式化又会让部分历史文件进入暂存区，因此本次一次性格式基线提交使用了：

```bash
SKIP=oat-check git commit -m "style: apply pre-commit formatting"
```

该命令只对这一次提交跳过 `oat-check`，其他 hook 仍然执行；它不会删除或永久禁用 OAT。后续普通提交仍应使用：

```bash
git add -A
pre-commit run
git commit -m "提交说明"
```

格式基线提交之后，CI 点名的 13 个模板脚本已经补齐 CANN License Header，根目录许可证正文 `LICENSE` 也已从 OAT Header 检查中精确排除。当前 PR 变更文件的 OAT 检查已经通过，不再依赖跳过 OAT。

如果要求 PR 达到全仓 OAT 通过，则不能使用跳过方案，需要先处理报告中的许可证头和二进制文件策略问题，再执行：

```bash
pre-commit run --all-files
```

直到 OAT 显示 `Passed`。

## OAT 存量问题治理结果

后续合规清理已处理上述 20 项历史问题：为 17 个源码和脚本模板以及 `.devcontainer/Dockerfile` 补充 CANN License Header，并在 OAT 二进制文件策略中仅过滤文档使用的 PNG 图片。其他二进制文件仍会被检查。

治理完成后执行 `pre-commit run --all-files`，全部检查通过，OAT 实际检查并通过 344 个文件。
