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

The first plug-in solves a long-standing annoyance: Winamp visualizers normally
react to files Winamp itself is playing, or to a recording input through
`linein://`. The loopback plug-in instead captures the audio Windows is already
sending to your default speakers or headphones and passes a visualization-only
copy into Winamp. That means Spotify, YouTube, games, browsers, and other apps
can drive AVS or MilkDrop without a software mixer.

It works after applications have decoded their audio, so the original codec or
compressed bitrate does not matter. Common Windows PCM and floating-point mix
formats are accepted, and source rates through 384 kHz are normalized to a
steady legacy-friendly visualization cadence.

## Future direction

This repository can grow to include other focused Winamp work, such as:

- modern Windows compatibility adapters;
- visualization and preset compatibility tools;
- high-refresh visualization experiments;
- utilities that preserve and extend classic Winamp workflows.

Only items actually present in the repository should be considered released.

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
