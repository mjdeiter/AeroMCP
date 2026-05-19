# Changelog

All notable changes to AeroMCP are documented here.

## [1.1.0] - 2026-05-19

### Added
- Model selector: Fast (Gemini 2.5 Flash), Thinking (Flash with thinking budget), Pro (2.5 Pro — requires billing)
- Draggable sidebar splitter (150–400px range)
- Image attachment support via zenity file picker (PNG, JPG, GIF, WebP)
- Self-identification: model correctly reports its name when asked
- System prompt with ELITEBOOK context baked in
- AeroMCP logo as window icon (32x32 RGBA)
- Console auto-scroll only on new output — manual scroll now works freely

### Fixed
- Drag handle no longer shows stray line when not hovered
- Pro model greyed out and re-enabled with billing note
- Console scroll no longer fights user input

### Changed
- Upgraded from Gemini 2.0 Flash to Gemini 2.5 Flash
- Switched to OpenClaw project API key for working free tier quota

## [1.0.0] - 2026-05-17

### Added
- Initial release
- Chat interface with session management (SQLite)
- Integrated bash console with ▶ Run buttons on code blocks
- Gemini API integration with streaming response support
- API key field with persistent config storage
- Dark theme UI with Hack font
