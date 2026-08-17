# chelper-cli —— 我的世界基岩版命令辅助器（deb 版）

基于 [CHelper](https://github.com/Yancey2023/CHelper) 开源内核 (GPLv3) 的命令行命令辅助工具。

## 功能

- **命令补全**：输入命令时实时给出补全建议（含描述）
- **语法检查**：显示命令中的语法错误及位置
- **命令结构**：展示当前命令的语法结构
- **参数提示**：显示当前光标处的参数说明
- **旧语法转换**：旧版指令（如 `setblock 0 0 0 stone 1`）转为新版语法
- **交互式 REPL**：`mc>` 提示符，`/s N` 应用建议

## 安装

```bash
sudo apt install ./chelper-cli_0.1.0-1_amd64.deb   # x86_64
sudo apt install ./chelper-cli_0.1.0-1_arm64.deb   # ARM64 (树莓派等)
```

## 使用

```bash
chelper                          # 交互模式
chelper --check "give @p diamond_sword 1"    # 语法检查
chelper --suggest "kill @"
chelper --old2new "setblock ~ ~ ~ stone 1"
```

## REPL 命令

| 命令 | 说明 |
|------|------|
| `:q` | 退出 |
| `:s <N>` | 应用第 N 条补全建议 |
| `:h` | 帮助 |

## 构建

仓库自带 GitHub Actions workflow（.github/workflows/build-deb.yml），
推送后自动为 amd64 / arm64 构建 deb 包。

本地构建：

```bash
sudo apt install g++ libfmt-dev libspdlog-dev libxxhash-dev rapidjson-dev libutf8cpp-dev
g++ -std=c++20 -O2 -D_GNU_SOURCE \
  -Iinclude -Ibuild-include -Isrc -I/usr/include/utf8cpp -Iserialization-include \
  $(find src/chelper -name '*.cpp' | sort) src/main.cpp \
  -o build/chelper -lspdlog -lfmt -lxxhash -pthread
```

## 资源

- `resources/release-vanilla-1.21.132.1.cpack` — 正式版命令语法数据（基岩版 1.21.132.1）
- `resources/blockFixData.json` — 方块数据值转方块状态对照表

## 许可

GPLv3，与 CHelper 相同。源码中对 CHelper 有两处 fmt 10 兼容性补丁
（`src/chelper/util/Profile.h`：`fmt.str` → `fmt.get()`、`fmt::vargs` → `fmt::make_format_args`）。
