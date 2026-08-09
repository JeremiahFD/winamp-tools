# Winamp Tools

An homage to Winamp, and a home for plug-ins and small tools that help bring it
into the modern era while keeping the things we love about it functional.

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

## Future direction

This repository can grow to include other focused Winamp work, such as:

- modern Windows compatibility adapters;
- visualization and preset compatibility tools;
- high-refresh visualization experiments;
- utilities that preserve and extend classic Winamp workflows.

Only items actually present in the repository should be considered released.

## In development: Live Stations

The next plug-in is a native Winamp Media Library station browser intended to
replace the practical function lost when the legacy Online Services radio page
returns HTTP 500. It searches the open Radio Browser directory at runtime,
discovers mirrors instead of hard-coding one server, filters broken entries,
and sends a selected stream to Winamp without erasing the existing playlist.

It also exposes a separate experimental video list. The directory can identify
H.264, MP4, and FLV-labeled streams, but playback still depends on Winamp's
installed input plug-ins; modern HLS video should not be assumed to work in
stock Winamp.

This work is not released yet. The source, build instructions, current test
status, and planned installation steps are in
[`live-stations`](live-stations/) and the
[step-by-step Live Stations guide](LIVE_STATIONS_GUIDE.md). A public download
will wait until the DLL has also been exercised in a real Winamp installation.

## Related work in progress

We are also working on an independent **Synced Visualizer** project. It is a
modern, host-neutral visualizer SDK and set of standalone apps rather than a
Winamp replacement. The direction includes:

- built-in spectrum, waveform, and radial visualizers;
- support for multiple rendering engines over time, including projectM-style
  MilkDrop rendering, portable AVS work, and optional web renderers such as
  Butterchurn where licensing permits;
- standalone Android, Android TV / NVIDIA Shield, and future Windows hosts;
- high-refresh rendering for modern displays;
- synchronized multi-device scenes where each screen can render a different
  visualization from the same musical feature clock;
- sharing derived timing and audio features between devices instead of copying
  or transmitting protected music.

That work is still experimental and is not included in this public repository
yet. Winamp Tools is the practical public home for smaller Winamp-compatible
plug-ins and utilities as they become ready.

## AI assistance

The first plug-in was created as a side project with OpenAI Codex assisting
with implementation, build automation, testing, and documentation.

## Disclaimer

This is an independent community project. It is not affiliated with or
endorsed by Winamp. Winamp and related names may be trademarks of their
respective owners.

## License

Code in this repository is available under the [MIT License](LICENSE).
