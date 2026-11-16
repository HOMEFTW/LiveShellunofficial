# Changelog

## 1.8.0 — 2026-11-17

This unofficial release builds on ZiZc3/LiveShell at `0cf248ea5a4d92026ebbbdbe0963116473a67791`. Original project by AZiZ (ZiZc3); fork maintained by HOMEFTW. GPL-3.0 and third-party notices are retained.

- Add native PNG / JPEG static wallpapers with aspect-preserving, centred cropping.
- Give videos, pictures and MP3 background music separate entry points and storage folders: `video`, `pictures` and `music`.
- Keep music selection global, with independent volume and single-track / ordered list looping in Settings.
- Decode MP3 in the payload and output PCM from SceShellUI's MAIN AudioOut port, reporting playback only after the receiver confirms submission.
- Add Simplified Chinese (default) / English, saved on the console and shared by both pages.
- Update the project name, application title, version display and About page with fork ownership and original author credits.

When upgrading, restart the PS5 or ShellUI before loading the new ELF so the new injected module starts. Background music plays while a picture is visible on the home screen; it pauses in the console's LiveShell management window or when the picture is hidden.

The preceding build received user confirmation that music playback worked. The 1.8.0 build is checked locally and compiled for PS5; this does not replace testing this exact ELF on a PS5.
