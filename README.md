# AeroMCP

![AeroMCP Logo](logo.png)

A native desktop AI assistant for Linux, powered by Google Gemini and built with Dear ImGui. Designed to work on any Linux system.

## Features

- **Fast, Thinking, and Pro model modes** — switch between Gemini 2.5 Flash, Flash Thinking, and 2.5 Pro
- **Integrated console** — run bash commands directly from the chat interface with ▶ Run buttons on code blocks
- **Image attachments** — attach PNG/JPG/WebP images to your messages via file picker
- **Persistent sessions** — conversation history stored in SQLite, survives restarts

## Requirements

- Linux (tested on CachyOS/Arch)
- Dear ImGui (included via `../fan-watch/imgui/`)
- GLFW3
- libcurl
- nlohmann/json
- SQLite3
- OpenGL

## Build

```bash
make -j$(nproc)
```

## Usage

1. Launch `./AeroMCP`
2. Paste your Gemini API key in the bottom-left field
3. Start chatting

> **Note:** Fast and Thinking modes work on the Gemini free tier. Pro mode requires a billing-enabled project.

## Configuration

Config is stored at `~/.config/aeromcp/config.ini`. The SQLite database lives at `~/.config/aeromcp/aeromcp.db`.

## License

MIT
