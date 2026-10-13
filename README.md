
# Psittacula

A lightweight terminal-based AI coding agent that was written to run with local **llama.cpp**, but it can also work with 
OpenAI-compatible gateways.

Psittacula streams answers in real time, shows model reasoning, and gives the model a multi-tool
environment: it can read, search, create and modify files, run shell commands and fetch web pages —
**only inside a sandbox directory you control**.

```
_______________________________________________________
      / __\ ___\//_ _/_ _// | /__\//  ///   / |
     / /_// /  // //  // //||//  //  ///   //||
    / ___/__ \// //  // //_||/  //  ///   //_||
   / /  ____\// //  // /___ |\_//__///__ /___ |
 __\/__/____//_//__//_//___||_/\___/____\/___||__
```
---

## Features

- **Local-first**: designed against the llama.cpp server (`/v1/chat/completions`, `/health`,
  `/slots`, reasoning chunks, per-token timings), tolerant of other OpenAI-compatible gateways.
- **Streaming SSE output** with a live spinner, token/context usage bar.
- **Reasoning display**: `reasoning_content` / `reasoning` / `thinking` chunks are rendered
  and can be hidden (`/reasoning false`).
- **Agent tools** (14): file create/read/modify/search/list/remove/copy/move, directory
  create/delete, binary files via base64, web fetch, shell commands.
- **Sandbox**: all file tools and command working directories are confined to the *project
  directory*; escapes are rejected with a `permission_denied` tool error the model can understand.
- **Confirmation gate**: shell commands always ask (y/n); non-GET web requests ask as well.
- **Undo / redo** for file and directory tools (`/undo`, `/redo`).
- **Context management**: autosaved dialogue (`logs/dialogue_*.txt`), dump / restore, context
  clearing and **LLM summarization compression** (`/compress`) with tool-result purging.
- **UTF-8 support**: console code pages, ANSI output and child-process output are converted so that
  a request body is always well-formed JSON (Cyrillic consoles, CP866 tool output, etc.).
- **Cross-platform TUI**: raw-mode terminal UI for Windows and POSIX (arrows, PageUp/PageDown,
  resize handling, `/`-command and `@`-file autocomplete).

---

## Building

Requirements:

- CMake **≥ 3.25**
- C++17 compiler (MSVC 2022, MinGW-w64, GCC, Clang)
- Dependencies: **libcurl**, **zlib**, **RapidJSON** + threads

With [vcpkg](https://vcpkg.io) as an example:

```sh
vcpkg install curl zlib rapidjson
```

Configure and build (out-of-source build is recommended):

```sh
cmake -S . -B Build/devel -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build Build/devel --config Release
```

Resulting binary: `Build/Release/bin/psittacula-X.X.X` (name carries the version from `version.txt`).
On Windows the runtime needs `libcurl.dll` / `zlib1.dll` next to the executable or in `PATH`
(`-d` variants for the Debug).

---

## Running

```sh
psittacula                                   # defaults (see directory layout below)
psittacula workdir=~/Documents/MyPsittacula  # custom workdir
psittacula project=~/code/my-project         # custom sandbox (project) directory
psittacula workdir=... project=...           # both
```

On first start the workdir structure is created and, if no model connections exist yet, the
**model creation wizard** starts automatically.

### Directory layout

```
%USERPROFILE%/Documents/Psittacula      (Windows default workdir)
$HOME/Psittacula                        (POSIX default workdir)
├── models/      *.txt model connection files
├── settings/    agent rules (system prompts); defaults are created on first run
├── projects/    default sandbox for the agent's file tools
└── logs/        dialogue log files (dialogue_*.txt) and dumps
```

---

## Connecting a model

Models are plain text files in `models/` (`key = value`, `#` comments). Created by `/model create`
or by hand:

```ini
# Model name
name=qwen2.5-coder-7b-instruct

# Host: concatenated with the chat completion endpoint for requests
host=http://localhost:8080

# Chat completion endpoint (optional; default depends on context_size, see below)
# chat_endpoint=/v1/chat/completions

# Context size (optional).
# Leave empty for llama.cpp: the endpoint defaults to /v1/chat/completions and the
# real context window is read from the /slots endpoint after every response.
# Set a value for other gateways: the endpoint defaults to /chat/completions.
# context_size=32768

# Bearer key: taken from the environment only, never stored in this file.
# Set the variable once (Windows: setx VAR_NAME sk-... — future shells,
# POSIX: export VAR_NAME=sk-...), then reference it here:
api_key_env=VAR_NAME
```

Endpoint selection summary:

| `context_size` | `chat_endpoint` | Request goes to |
|----------------|-----------------|-----------------|
| empty (llama.cpp) | empty | `<host>/v1/chat/completions`, context read from `/slots` |
| set | empty | `<host>/chat/completions` |
| any | set | `<host>/<chat_endpoint>` (explicit wins) |

`/model` runs a connectivity probe before connecting and reports per-endpoint results
(`/health`, `/slots`, `/v1/models`, `/models`, `/api/v1/models`), so a wrong host or key is
visible immediately. Switching models keeps the current dialogue.

---

## Chat commands

| Command | Description |
|---------|-------------|
| `/help`, `/h` | List all commands. |
| `/model [create]` | Choose a model connection (interactive), or create a new one. Does not clear context. |
| `/tools` | List the tools available to the agent. |
| `/reasoning true|false` | Show/hide the model's reasoning text. |
| `/undo`, `/redo` | Undo/redo the last file/directory tool action. |
| `/clear` | Clear the dialogue (keeps the system prompt). |
| `/compress` | Summarize the older context into a compact summary (keeps the system prompt, the summary and the last messages). |
| `/dump [name]` | Save the full dialogue JSON to `logs/`. |
| `/dump_text [name|path]` | Save the dialogue as readable text (`.md`). |
| `/restore <name>` | Restore a dialogue from `logs/`. |
| `/rules [show]` | Load agent rules (system prompt) from `settings/*.txt`, or show the current one. |
| `/project <path>` | Change the sandbox (project) directory used by file tools. |
| `/purge [name]` | Delete dialogue dumps from `logs/` (all, or one file). |

### Keyboard

| Key | Action |
|-----|--------|
| `Enter` | New input line; **`Enter` on an empty line sends** the typed message. |
| `/` … | At line start: interactive command autocomplete. |
| ` @` | Filename autocomplete (inserts the full sandbox path). |
| `↑` / `↓` | Scroll the transcript, or navigate an open autocomplete list. |
| `PageUp` / `PageDown` | Scroll the transcript by a page. |
| `ESC` | While a query streams: interrupt it. When idle: exit dialog. In menus: leave the menu. In a confirm box: deny. |
| `Ctrl+C` / `Ctrl+D` | Interrupt a running query. |

---

## Agent tools

All path arguments accept an **absolute path or a path relative to the project (sandbox) directory**.

| Tool | Description |
|------|-------------|
| `list_files` | Directory listing; recursive / hidden / summary-only modes, depth and size caps. |
| `read_file` | Read a file as text, or as base64 (`binary=true`). Caps: 256 KiB text (truncated, reported), 1 MiB binary (refused above the limit). |
| `create_file` | Create a text file (overwrites with backup for undo). |
| `create_file_binary` | Create a binary file from base64 content. |
| `modify_file` | Write with modes: `overwrite`, `insert`, `append`, `replace_range`. |
| `file_content_modify` | Surgical replace: `old_str` must occur exactly once. |
| `remove_file`, `copy_file`, `move_file` | File operations, all sandbox-checked (source **and** destination). |
| `create_dir`, `delete_directory` | Directory create; delete of an **empty** directory only. |
| `file_search` | Plain-text or regex search over a tree; binary files skipped; match/size caps; `truncated` flag. |
| `web_fetch` | HTTP GET/POST with size caps and optional headers/body. Confirmation is requested for POST requests, custom headers or loopback URLs. |
| `run_command` | Shell command (`cmd.exe` on Windows, `/bin/sh` on POSIX) with a confirmation prompt, working-directory sandbox check, timeout (default 120 s, max 1 h) and per-stream output caps (default 100 KB). |

### Safety model

- File tools operate **only** inside the project directory; everything else returns
  `permission_denied` (the model is told the sandbox path and usually self-corrects).
- `run_command` **always** asks for confirmation; the command, its working directory and its
  output are shown in the console.
- Destructive file actions are undoable via `/undo` until the session ends.

---

## llama.cpp notes

- Works out of the box: leave `context_size` empty in the model file.
- The llama.cpp `timings` / `usage` chunk data is parsed to display tokens, context usage and cost;
  `reasoning_content` is displayed as dimmed reasoning text.
- `/slots` provides the real `n_ctx`, so the context-usage bar is accurate even when the model
  config does not state the window size.

## Other gateways

- Any OpenAI-compatible server works: set `context_size` (and `chat_endpoint` if its path differs
  from `/chat/completions`), plus `api_key_env` if required.
- `usage.total_tokens / prompt_tokens / completion_tokens / cost` are parsed from the trailing
  usage chunk when the gateway sends one.

---

## Troubleshooting

| Symptom | Check |
|---------|-------|
| “Cannot connect to the selected model” | `/model` prints per-probe details (HTTP status, latency, curl error). Verify the host and that the server is running. |
| 404 on chat requests | Wrong endpoint: set `chat_endpoint` explicitly (e.g. `/v1/chat/completions`). |
| “API key rejected” | For remote gateways set the API key as the environment variable referenced by `api_key_env` in the model file (keys are not read from files); remove `api_key_env` for keyless local servers. Check the variable is set in this shell (`setx` only affects **future** shells). |
| Tool error `permission_denied` | The path is outside the sandbox — point `/project` at the right directory or use relative paths. |
| Context full / degraded answers | `/compress` (summarizes older history) or `/clear`. |
| Broken terminal after a crash (POSIX) | `stty sane`. Psittacula restores the console on normal exit and on SIGINT/TERM/HUP, but `kill -9` leaves the terminal in raw mode. |
| Garbled non-ASCII tool output | Output is converted from the console code page to UTF-8 automatically; if a tool emits raw binary, prefer `read_file` with `binary=true`. |

---

## Architecture

Three C++17 static-library/executable modules, built by one CMake root project:

| Directory          | What it is |
|--------------------|------------|
| `psittacula-tui/`  | The terminal application: screen/panels/input, keyboard decoding, chat commands, model & rules pickers. |
| `psittacula-cli/`  | The engine: `AiClient`, libcurl HTTP/SSE client, dialogue body (RapidJSON), connectivity checker, working-dir/sandbox singleton, undo/redo manager. |
| `psittacula-tools/`| The agent tool implementations (one class per tool, registered into a dispatcher). |

```
version.txt                 single source of the version (A_VERSION_MAJOR/MINOR/PATCH)
cmake/modules/FsUtils.cmake reads version.txt into CMake variables
Build/<cfg>/bin             executables
Build/<cfg>/lib             built static libraries
```

---

## Source layout

```
psittacula-cli/include/   public engine headers (AiClient, Model, console_writer, utf8_util, ...)
psittacula-cli/src/       AiClientImpl, SSE streaming (ChunkProcessor), DialogueBody, sandbox (WorkingDir)
psittacula-tui/src/       App main loop, Screen/Panels, Keyboard, chat commands
psittacula-tools/src/     one Tool class per agent tool
```

Key internal contracts:

- Everything that enters a request body passes a UTF-8 sanitize gate (`utf8::Sanitize`) — a single
  ill-formed byte otherwise makes servers reject the whole request.
- `AiClientImpl` runs on a worker thread; the UI thread only sets the cancel flag and renders.
  The confirmation dialog is the one deliberate cross-thread handshake (mutex + condition variable).
- Tool responses are JSON strings; parse them with RapidJSON, never by string concatenation.

---

*Psittacula — any of various mostly tropical parrots. This one sits in your terminal.*
