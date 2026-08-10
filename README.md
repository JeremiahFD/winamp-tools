# Winamp Tools

An homage to Winamp, and a home for plug-ins and small tools that help bring it
into the modern era while keeping the things we love about it functional.

[![Latest release](https://img.shields.io/github/v/release/JeremiahFD/winamp-tools?label=release)](https://github.com/JeremiahFD/winamp-tools/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-5b9bd5.svg)](LICENSE)
[![Donate with PayPal](https://img.shields.io/badge/Donate_with-PayPal-0070BA?style=flat&logo=paypal&logoColor=white)](https://www.paypal.com/donate/?business=CE2H5ZMBQKY9E&no_recurring=0&item_name=Thank+you+for+your+support%21+Your+donations+help+me+continue+to+do+the+work+I+love%21+I+hope+you+enjoy.+Be+sure+to+share%21&currency_code=USD)

This is a hobby project. I love Winamp and want to keep experimenting with ways
to make its classic features useful with modern Windows, audio devices, and
workflows.

## Available now

### WASAPI Loopback Input

Use AVS, MilkDrop, and other Winamp visualization plug-ins with audio playing
anywhere on the current Windows output device. No Stereo Mix, virtual cable, or
software mixer is required.

Download the ready-to-use ZIP from the repository's **Releases** page, or see
[`wasapi-loopback-input`](wasapi-loopback-input/) for source and build details.

> **New to plug-ins or Windows folders?** Open the
> [complete beginner installation guide](BEGINNER_INSTALL_GUIDE.md). It
> explains what the downloaded file is, where to put it, what `loopback://`
> means, and how to start a visualizer.

> **v0.1.1 bug-fix notice:** The original v0.1.0 download did not work
> reliably in real Winamp testing. Opening or stopping `loopback://` could
> crash Winamp, and the visualizer could remain black. Do not use v0.1.0.
> The cleanup and live-input behavior have been corrected in v0.1.1, and the
> replacement DLL has been verified in Winamp 5.92 with live Realtek output.

## Quick setup

1. Download the current ZIP from the [latest release](https://github.com/JeremiahFD/winamp-tools/releases/latest).
2. Copy `in_svloopback.dll` into Winamp's `Plugins` folder.
3. In Winamp, press **Ctrl+L**, enter `loopback://`, and start a visualizer.

For the click-by-click instructions, explanations, and troubleshooting, open
the [complete beginner installation guide](BEGINNER_INSTALL_GUIDE.md).

The first plug-in solves a long-standing annoyance: Winamp visualizers normally
react to files Winamp itself is playing. Winamp's older `linein://` feature uses
a recording input and is separate from this project. This loopback plug-in
instead captures the audio Windows is already sending to your default speakers
or headphones and passes a visualization-only copy into Winamp. That means
Spotify, YouTube, games, browsers, and other apps can drive AVS or MilkDrop
without a software mixer.

It works after applications have decoded their audio, so the original codec or
compressed bitrate does not matter. Common Windows PCM and floating-point mix
formats are accepted. The plug-in passes Winamp the Windows output device's
native mix rate instead of assuming 44.1 or 48 kHz; automated rate handling
checks cover 8 kHz through 384 kHz. Very old visualization plug-ins or presets
may still have their own sample-rate limitations.

## Project map

Only **WASAPI Loopback Input v0.1.1** is released today. Everything else below
is development work or a possible direction. Names, features, and release plans
may change; inclusion here is not a promise that a feature will ship.

| Project | Status | Current purpose and possible direction |
| --- | --- | --- |
| **Winamp AudioBridge** | In development | A friendlier successor to Loopback Input that may add a Media Library page, output-device selection, and clearer start/stop controls while preserving `loopback://`. Later experiments may explore metadata, media controls, and opt-in Winamp EQ/DSP processing. |
| **Live Stations** | In development | A station-directory browser that fetches current providers instead of bundling thousands of stream links. Planned favorites are user-owned folders that can resolve a provider's stable station ID to its current URL. Categories, local browsing, and codec-aware playback are still being designed. |
| **AVS Compatibility** | Research | May improve the experience of finding, testing, and organizing legally distributable AVS presets and compatibility fixes. Existing Winamp/third-party code and preset licenses must be respected. |
| **Winamp Plugin Manager** | Concept | May install and update Winamp Tools releases, then optionally help users obtain third-party plug-ins from official sources with confirmation, hashes, backups, and rollback. It will not silently redistribute software we do not own. |
| **Station Chat** | Concept, optional | A separate community-driven chat plug-in that may open a room for the station or stream currently playing. Decentralized history, privacy, IP exposure, abuse controls, and stable room identity need careful design before implementation. |
| **Synced Visualizer** | Separate experiment | A modern visualizer project exploring built-in scenes, multiple rendering engines, high-refresh displays, and synchronized multi-device visuals. It is not included in Winamp Tools releases. |

## Ideas, feedback, and bugs

- Share ideas in [Ideas and feature requests](https://github.com/JeremiahFD/winamp-tools/discussions/3).
- Ask for help or report a problem in [Bug reports and compatibility help](https://github.com/JeremiahFD/winamp-tools/discussions/4).
- Join the original [Winamp Tools welcome discussion](https://github.com/JeremiahFD/winamp-tools/discussions/1).

Please include the Winamp Tools version, Winamp version, Windows version, and
the exact steps that led to a problem. Do not post credentials or private logs.

## AI assistance

The first plug-in was created as a side project with OpenAI Codex assisting
with implementation, build automation, testing, and documentation.

## Support

Winamp Tools remains free and MIT-licensed. If it helps you and you want to
support continued hobby development and compatibility testing, you can
[donate through PayPal](https://www.paypal.com/donate/?business=CE2H5ZMBQKY9E&no_recurring=0&item_name=Thank+you+for+your+support%21+Your+donations+help+me+continue+to+do+the+work+I+love%21+I+hope+you+enjoy.+Be+sure+to+share%21&currency_code=USD). Donations are optional.

## Disclaimer

This is an independent community project. It is not affiliated with or
endorsed by Winamp. Winamp and related names may be trademarks of their
respective owners.

## License

Code in this repository is available under the [MIT License](LICENSE).
