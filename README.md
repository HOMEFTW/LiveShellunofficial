<p align="center">
  <img src="assets/icon0.png" width="140" alt="LiveShellunofficial">
</p>

<h1 align="center">LiveShellunofficial</h1>

<p align="center"><b>Video and static picture wallpapers for the PS5 home screen.</b></p>

<p align="center">
  <img src="https://img.shields.io/badge/version-1.8.0-white?style=flat-square" alt="version">
  <img src="https://img.shields.io/badge/platform-PS5-white?style=flat-square" alt="platform">
  <img src="https://img.shields.io/badge/video-H.264%20%7C%20HEVC%20%E2%89%A44K-white?style=flat-square" alt="video">
  <img src="https://img.shields.io/badge/license-GPL--3.0-white?style=flat-square" alt="license">
</p>

---

An unofficial fork of [LiveShell Engine](https://github.com/ZiZc3/LiveShell) by **AZiZ (ZiZc3)**, maintained by **HOMEFTW**. Version **1.8.0** adds static pictures, independent MP3 background music and Simplified Chinese / English settings. See [CHANGELOG.md](CHANGELOG.md) for changes and [Releases](https://github.com/HOMEFTW/LiveShellunofficial/releases) for the compiled ELF and complete packages.

这是由 HOMEFTW 维护的非官方分支，原作者为 AZiZ（ZiZc3）。支持静态图片壁纸、独立背景音乐、单曲／列表循环，以及设置中的中英文切换。下载与中文说明见 [GitHub Releases](https://github.com/HOMEFTW/LiveShellunofficial/releases)。

## Features

- **Pictures**: PNG and JPEG wallpapers, displayed by the native image widget without making a video. Pictures fill the screen with a centred crop that preserves their aspect ratio.
- **Separate libraries**: Videos, Pictures and Background music each have their own entry point. Phone / PC uploads go into the corresponding folder automatically.
- **Background music**: one global MP3 selection for all pictures, with its own volume. Settings offers **Single-track loop** or **List loop** (in library order, returning to the first track). Music pauses when the picture is hidden or a video is selected; it is not assigned to individual pictures.
- **Languages**: Simplified Chinese (default) and English. Change **Settings → Language** on either page; the choice is saved on the console and shared with the phone / PC page.
- **Discover**: hundreds of animated wallpapers to download right on the PS5, by category (Anime, Games, Movies, ...) the list updates by itself.
- **Library** tab with all your games and apps, to give each one its own video (experimental).
- **PS Store only**: the video shows only on the PlayStation Store tile, with its own video.
- **Playlists**: your wallpaper changes by itself through the videos you pick.
- **Sound**: videos with sound play it.
- **Game music**: a game's own theme keeps playing when its video has no sound or Sound is on Mute
- **Your clips** from internal storage or a USB drive
- **Looks**: Dim, Vignette, Haze, all combinable
- **Hide in Settings**: Settings keeps the normal PS5 background
- **Pause while a game runs**, so games get the video decoder
- **Parallax** effect when moving between games
- **Random video** on every start
- **Rest mode**: comes back on its own after waking the PS5
- **Home-screen app** to pick, delete and control everything
- **Phone / PC page** to upload, play and delete videos

## Install

Needs a jailbroken PS5 with a payload loader.

1. Copy files into the matching folder below, on internal storage or a USB drive. You can also upload them from the matching tab on the phone / PC page.
2. Load `liveshell.elf`.
3. Open **LiveShellunofficial** on the home screen (Media).

Videos: H.264 or HEVC, 8-bit, up to 4K, with or without sound. 
4K / 2K play as the main wallpaper or in PS Store only mode, and hide while a game is open.

Pictures: `.png`, `.jpg` or `.jpeg`, up to **32 MB** and **4096 pixels per side**. PNGs must be 8-bit and non-interlaced; JPEGs must be 8-bit RGB or grayscale (baseline or progressive). The header is checked before upload completion / selection; the PS5 performs the actual image decode.

Music: `.mp3`, mono or stereo, 8–48 kHz, up to **64 MB**. Choose a track or **No music** from **Background music**. Choose **Settings → Background music → Single-track loop / List loop**. Music and repeat / volume settings are saved globally in `music.txt`.

Background music is decoded in the payload and sent over loopback UDP to **SceShellUI**, which opens a MAIN AudioOut port for audible home-screen output. The page reports **Playing** only after the receiver successfully submits audio; missing connections or native output errors are shown in the music status. An accepted audio submission still requires on-console listening to verify the device output.

Choose **设置 → 界面语言 → 简体中文 / English** (English: **Settings → Language**) to switch languages. The console saves `language=zh-CN` or `language=en` in `config.txt`. Translations are embedded in the ELF and do not need an Internet connection. File names and paths keep their original text.

```text
LiveShell/                 # internal: /data/LiveShell; USB: liveshell
├── video/                 # MP4 videos and their existing thumbnail sidecars
├── pictures/              # PNG / JPG / JPEG wallpapers
└── music/                 # MP3 background music
```

Old MP4s directly inside the USB `liveshell` folder remain readable. Picture files belong in `pictures`, so existing video thumbnail PNGs are never listed as wallpapers or overwritten by picture uploads.

Pictures stay on screen until you choose another wallpaper or a timed playlist advances. **Auto-next** / **when each video ends** apply to videos; a playlist containing pictures needs a timed interval. Random selection on startup stays within the selected wallpaper type.

When upgrading from an older ELF, restart the PS5 / ShellUI before loading this build, so the previously injected module is not reused. This custom build requires PS5 hardware testing; host tests and a successful payload build do not establish firmware compatibility or on-console image / audio behaviour.

## Discover

1. Open the **Discover** tab.
3. Download and Press **Use**: it plays on the home screen or per game. Downloaded wallpapers also appear in **All** tab.

## A video for a game (experimental)

1. Open the **Library** tab and pick a game or app.
2. Choose its video. **Reset** gives it back the default background.

Keep all videos in one place (internal **or** USB) for the quickest switching.

**A video for every tile**

- 🎮 **Games**: PS5 and PS4
- 🧩 **Homebrew and web-app tiles**: Payload Manager, etaHEN Toolbox, and more
- 🛍️ **PlayStation Store**: with PS Store only.

## Folders on the PS5

| Path | What |
|---|---|
| `/data/LiveShell/video/` | Your videos |
| `/data/LiveShell/pictures/` | Your picture wallpapers |
| `/data/LiveShell/music/` | Your background music |
| `<USB>/liveshell/video/` | Videos on a USB drive |
| `<USB>/liveshell/pictures/` | Pictures on a USB drive |
| `<USB>/liveshell/music/` | Music on a USB drive |
| `/data/LiveShell/config.txt` | Settings |
| `/data/LiveShell/music.txt` | Global music choice, volume and repeat mode |
| `/data/LiveShell/liveshell.log` | Log |

## Building

Linux or WSL, with the [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) (this build was checked with v0.45 and Clang 18.1.3) and the [OnionHEN](https://github.com/aydencharles/onionHEN) v0.9.0-beta2 source tree.

```sh
ONIONHEN=~/onionHEN-beta2 PS5_PAYLOAD_SDK=~/ps5-payload-sdk tools/build.sh
```

`src/` is copied into OnionHEN as `source/liveshell` and built as the `liveshell` target.

Focused host checks (Python 3.9+, a C++20 `g++`, Node.js):

```sh
python tests/run_host_tests.py
```

The music checks run the real MP3 decoder / playback state with adapted filesystem and AudioOut calls. They cover loop modes, visibility pause, mute, settings persistence, receiver acknowledgments, native output errors and cleanup. Language checks cover both languages, dynamic messages and content preservation. These checks do not play audio on a PS5.

## Credits

- **[OnionHEN](https://github.com/aydencharles/onionHEN/)**: payload injector, detour library and ShellUI groundwork
- **[PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk)**: toolchain and PS5 libraries
- **[dr_mp3](https://github.com/mackron/dr_libs)**: MP3 decoding (public domain / MIT-0; see `THIRD_PARTY_NOTICES.md`)

## License

GPL-3.0. See [LICENSE](LICENSE).
