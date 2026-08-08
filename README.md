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

> **v0.1.1 bug-fix notice:** The original v0.1.0 download did not work
> reliably in real Winamp testing. Opening or stopping `loopback://` could
> crash Winamp, and the visualizer could remain black. Do not use v0.1.0.
> The cleanup and live-input behavior have been corrected in v0.1.1, and the
> replacement DLL has been verified in Winamp 5.92 with live Realtek output.

## Install and use it (step by step)

> **Important:** type `loopback://` exactly. Do **not** type `linein://`.
> `linein://` belongs to Winamp's separate, older Line Input plug-in and does
> not start this WASAPI loopback plug-in. On some modern systems that old
> plug-in may also be unstable.

1. Open the [latest release](https://github.com/JeremiahFD/winamp-tools/releases/latest)
   and download `winamp-wasapi-loopback-v0.1.1-x86.zip`.
2. Open the downloaded ZIP. Inside it, find `in_svloopback.dll`.
3. Completely close Winamp.
4. Copy `in_svloopback.dll` into Winamp's plug-in folder. For a normal install,
   that folder is:

   ```text
   C:\Program Files (x86)\Winamp\Plugins
   ```

   Windows may ask for administrator permission when you copy the file. If
   Winamp was installed somewhere else, open that Winamp folder and then open
   its `Plugins` folder.
5. Start Winamp again.
6. In Winamp, press **Ctrl+L**. This opens Winamp's **Play URL** or **Open URL**
   box. This is where the address goes; do not type it into a web browser or a
   playlist search box.
7. Type the following into that box, then click **Open** or press **Enter**:

   ```text
   loopback://
   ```

8. Winamp's title or playlist should now show **System Output (WASAPI
   Loopback)**. Start music, a video, a game, or any other audio on the
   computer.
9. In Winamp, open **Options > Visualization > Select plug-in**, choose AVS,
   MilkDrop, or another installed visualizer, and start it. In many Winamp
   layouts, **Ctrl+Shift+K** also starts or stops the selected visualization.

Keep `loopback://` open in Winamp while the other app plays the sound. The
sound continues to come from that app; Winamp only receives a copy for the
visualizer, so silence from Winamp itself is normal.

If you change from speakers to headphones, Bluetooth, HDMI, or another Windows
output device, stop `loopback://` and open it again so the plug-in connects to
the new default output.

### If the visualizer does not move

- Confirm that Windows is actually playing the audio through its current
  default output device.
- Confirm that Winamp shows **System Output (WASAPI Loopback)** after opening
  `loopback://`.
- Close and reopen `loopback://` after changing audio devices.
- Try AVS or MilkDrop's built-in presets first. Some old third-party presets
  can crash or hang modern Winamp independently of this plug-in.

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
