# NexusWave

English | [简体中文](README.CN.md)

A TUI Agent built on [NexusForce](https://github.com/aurora250/NexusForce).

## Build

Requirements: CMake 3.20+, a C++20 compiler, and NexusForce.

an installed prefix at `NEXUSWAVE_NEXUSFORCE_PREFIX` (or `.deps/nexusforce`, produced by `python3 scripts/install_nexusforce.py`).

```bash
cmake --preset dev            # or: cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build --preset dev    # or: cmake --build build --parallel
ctest --preset dev            # or: ctest --test-dir build --output-on-failure
```

| CMake option                    | Default                    | Meaning                                                          |
|---------------------------------|----------------------------|------------------------------------------------------------------|
| `NEXUSWAVE_NEXUSFORCE_SOURCE`   | `../NexusForce`            | NexusForce checkout used when no prefix exists                   |
| `NEXUSWAVE_NEXUSFORCE_PREFIX`   | auto (`.deps/nexusforce`)  | Installed NexusForce prefix                                      |
| `NEXUSWAVE_BUILD_FROM_SOURCE`   | `OFF`                      | Force the source build even when a prefix exists                 |
| `NEXUSWAVE_NEXUSFORCE_MINIMAL`  | `ON`                       | Disable NexusForce tests/examples/benchmarks/docs in source mode |
| `NEXUSWAVE_VCPKG_INSTALLED_DIR` | `<source>/vcpkg_installed` | Reuse an existing vcpkg tree for the source build                |
| `NEXUSWAVE_BUILD_TESTS`         | `ON`                       | Build the NexusWave unit tests                                   |

## Configuration

| Variable              | Meaning                                                         |
|-----------------------|-----------------------------------------------------------------|
| `DEEPSEEK_API_KEY`    | API key                                                         |
| `NEXUSWAVE_BASE_URL`  | OpenAI-compatible base URL (default `https://api.deepseek.com`) |
| `NEXUSWAVE_MODEL`     | Model ID (default `deepseek-chat`)                              |
| `NEXUSWAVE_WORKSPACE` | Tool workspace root (default: current directory)                |

Sessions are written to `~/.nexuswave/sessions/*.jsonl`.

Modes: `default` (workspace writes allowed), `plan` (read-only), `full` (no prompts); switch with `shift+tab`, `ctrl+t`, or `/mode`.

## Usage

| Key             | Action                               |
|-----------------|--------------------------------------|
| `enter`         | Send / run command                   |
| `shift+enter`   | Newline                              |
| `esc`           | Close help / clear input / interrupt |
| `?`             | Help (when input is empty)           |
| `ctrl+o`        | Expand tool cards                    |
| `ctrl+m`        | Toggle math rendering                |
| `pgup` / `pgdn` | Scroll transcript                    |
| `ctrl+c` twice  | Quit                                 |

## Commands

| Command                       | Description                                                                         |
|-------------------------------|-------------------------------------------------------------------------------------|
| `/help`                       | Show key bindings and the command list                                              |
| `/clear`                      | Clear the transcript (keeps the current session)                                    |
| `/new`                        | Start a new session: clears the transcript and agent history, opens a new JSONL log |
| `/theme <name\|list>`         | Switch palette, or list the installed themes                                        |
| `/mode <default\|plan\|full>` | Set the permission mode (workspace write / read-only / no prompts)                  |
| `/model <id>`                 | Switch the model used by subsequent turns                                           |
| `/status`                     | Show session id, API-key state, model, mode, context and token usage                |
| `/math <on\|off>`             | Toggle LaTeX/math rendering                                                         |
| `/ask <text>`                 | Send text as a user turn through the agent runtime                                  |
| `/demo`                       | Insert an offline demo transcript (table, math, Mermaid, tool card)                 |
| `/export <path>`              | Export the transcript as Markdown to `path`                                         |
| `/cancel`                     | Cancel the running turn                                                             |
| `/about`                      | Show version, model and theme                                                       |
| `/quit`                       | Exit NexusWave                                                                      |
