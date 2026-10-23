# NexusWave

[English](README.md) | 简体中文

基于 [NexusForce](https://github.com/aurora250/NexusForce) 的 TUI Agent。

## 构建

依赖：CMake 3.20+、C++20 编译器、NexusForce。

`NEXUSWAVE_NEXUSFORCE_PREFIX` 指向安装前缀（或 `.deps/nexusforce`，由`python3 scripts/install_nexusforce.py` 生成）。

```bash
cmake --preset dev            # 或：cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build --preset dev    # 或：cmake --build build --parallel
ctest --preset dev            # 或：ctest --test-dir build --output-on-failure
```

| CMake 选项                      | 默认值                       | 说明                                            |
|---------------------------------|------------------------------|-------------------------------------------------|
| `NEXUSWAVE_NEXUSFORCE_SOURCE`   | `../NexusForce`              | 没有可用前缀时使用的 NexusForce 源码目录        |
| `NEXUSWAVE_NEXUSFORCE_PREFIX`   | 自动（`.deps/nexusforce`）   | 已安装的 NexusForce 前缀                        |
| `NEXUSWAVE_BUILD_FROM_SOURCE`   | `OFF`                        | 即使存在前缀也强制从源码构建                    |
| `NEXUSWAVE_NEXUSFORCE_MINIMAL`  | `ON`                         | 源码模式下关闭 NexusForce 的测试/示例/基准/文档 |
| `NEXUSWAVE_VCPKG_INSTALLED_DIR` | `<源码目录>/vcpkg_installed` | 复用已有的 vcpkg 安装树                         |
| `NEXUSWAVE_BUILD_TESTS`         | `ON`                         | 构建 NexusWave 单元测试                         |

## 配置

| 变量                  | 含义                                                   |
|-----------------------|--------------------------------------------------------|
| `DEEPSEEK_API_KEY`    | API 密钥                                               |
| `NEXUSWAVE_BASE_URL`  | OpenAI 兼容基础 URL（默认 `https://api.deepseek.com`） |
| `NEXUSWAVE_MODEL`     | 模型 ID（默认 `deepseek-chat`）                        |
| `NEXUSWAVE_WORKSPACE` | 工具工作区根目录（默认当前目录）                       |

会话写入 `~/.nexuswave/sessions/*.jsonl`。

模式：`default`（允许工作区写入）、`plan`（只读）、`full`（无提示）；通过 `shift+tab`、`ctrl+t` 或 `/mode` 切换。

## 用法

| 按键              | 操作                       |
|-------------------|----------------------------|
| `enter`           | 发送 / 运行命令            |
| `shift+enter`     | 换行                       |
| `esc`             | 关闭帮助 / 清空输入 / 中断 |
| `?`               | 帮助（输入为空时）         |
| `ctrl+o`          | 展开工具卡片               |
| `ctrl+m`          | 切换数学渲染               |
| `pgup` / `pgdn`   | 滚动转录记录               |
| 连按两次 `ctrl+c` | 退出                       |

## 命令

| 命令                          | 说明                                                   |
|-------------------------------|--------------------------------------------------------|
| `/help`                       | 显示按键与命令列表                                     |
| `/clear`                      | 清空转录记录（保留当前会话）                           |
| `/new`                        | 新建会话：清空转录与 Agent 历史，打开新的 JSONL 日志   |
| `/theme <名称\|list>`         | 切换调色板，或列出已安装主题                           |
| `/mode <default\|plan\|full>` | 设置权限模式（工作区可写 / 只读 / 不提示）             |
| `/model <id>`                 | 切换后续回合使用的模型                                 |
| `/status`                     | 显示会话 ID、密钥状态、模型、模式、上下文与 token 用量 |
| `/math <on\|off>`             | 开关 LaTeX/数学渲染                                    |
| `/ask <文本>`                 | 通过 Agent 运行时发送一条用户消息                      |
| `/demo`                       | 插入离线演示转录（表格、公式、Mermaid、工具卡）        |
| `/export <路径>`              | 把转录导出为 Markdown 文件                             |
| `/cancel`                     | 取消正在运行的回合                                     |
| `/about`                      | 显示版本、模型与主题                                   |
| `/quit`                       | 退出 NexusWave                                         |
