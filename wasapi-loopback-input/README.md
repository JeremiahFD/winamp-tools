# Winamp WASAPI Loopback Input

A small 32-bit Winamp input plug-in that sends audio from the current default
Windows output device to Winamp's visualization callbacks. It lets AVS,
MilkDrop, and other visualizers react to audio playing in browsers, streaming
apps, games, and other programs.

It uses shared-mode WASAPI loopback directly. You do **not** need Stereo Mix,
"Listen to this device," a virtual audio cable, or a software mixer.

This is a hobby project made by someone who loves Winamp and wanted this old
workflow to feel natural on a modern Windows laptop.

## How it works

When Winamp opens `loopback://`, the input plug-in:

1. asks Windows for the current default multimedia output device;
2. starts a shared-mode WASAPI loopback capture on that device;
3. converts common Windows mix formats to 16-bit stereo at the output device's
   native mix rate;
4. sends 576-frame PCM blocks to Winamp's spectrum and waveform visualization
   callbacks;
5. leaves the real audio playing through its original app and output device.

The plug-in is a bridge, not a mixer or player. It does not route the sound back
out of Winamp, so it does not create an echo and does not need monitoring.

```text
Browser / game / music app
           |
           v
Windows default speakers or headphones
           |
      WASAPI loopback
           |
           v
in_svloopback.dll -> Winamp visualization data -> AVS / MilkDrop
```

## Install

> **Important:** type `loopback://` exactly. Do **not** type `linein://`.
> `linein://` starts Winamp's separate, older Line Input plug-in rather than
> this WASAPI loopback plug-in, and that old plug-in may be unstable on some
> modern systems.

1. Download `winamp-wasapi-loopback-v0.1.1-x86.zip` from the Releases page.
2. Close Winamp.
3. Copy `in_svloopback.dll` into Winamp's `Plugins` folder, normally
   `C:\Program Files (x86)\Winamp\Plugins`.
4. Restart Winamp.
5. Press **Ctrl+L**, enter `loopback://`, and press Enter.
6. Start AVS, MilkDrop, or another visualization plug-in.

The Winamp title should show `System Output (WASAPI Loopback)`. Stop and reopen
`loopback://` after changing the default Windows output device.

### Picking MilkDrop presets

Click inside the MilkDrop window and press **L** to open its preset browser.
Press Space for another preset. AVS uses `.avs` presets; MilkDrop uses `.milk`
presets, so their collections are not interchangeable.

## What it captures

The plug-in captures normal shared-mode audio playing through the default
Windows multimedia output selected when `loopback://` starts. Audio routed to
another device, exclusive-mode streams, and protected content may not be
available to loopback capture.

The original media codec and compressed bitrate do not matter here. Spotify,
YouTube, MP3, AAC, FLAC, games, and other sources are decoded by their own
applications before Windows mixes them for the output device. This plug-in
captures that decoded Windows mix.

It does not save, transmit, or replay audio. It only supplies PCM samples to
Winamp's visualization interfaces.

## Compatibility and status

- Experimental version `0.1.1`.
- Built for 32-bit Winamp 5 on Windows 10/11.
- Tested live with Winamp 5.92, AVS, and MilkDrop 2.
- Version `0.1.0` did not work reliably in real Winamp testing and could crash
  while starting or stopping `loopback://`. Do not use it. Version `0.1.1`
  corrects the cleanup and live-input behavior and should fix that failure.
- Captures the first two output channels; mono is duplicated to stereo.
- Supports normal Windows shared-mode output mixes: unsigned PCM 8-bit, signed
  PCM 16/24/32-bit, and floating-point 32/64-bit.
- Passes the Windows device's native mix rate to Winamp instead of assuming
  44.1 or 48 kHz. Automated rate handling checks cover 8 kHz through 384 kHz;
  individual legacy visualizers or presets may have narrower limits.
- The DLL is unsigned, so Windows or antivirus software may warn about it.
- Legacy visualizer presets can still crash or hang Winamp independently of
  this input plug-in.

Validated release DLL SHA-256:

`9D8B9A8F9DEE66D3001D1E39042B8B0670C47FF6D4301DEDB6B4B2DF0EABE45A`

## Build and test

Requirements:

- Visual Studio Build Tools with the x86 C++ compiler;
- Windows 10/11 SDK;
- PowerShell.

From this directory:

```powershell
.\build.ps1 -Configuration Release
```

Outputs:

- `build\x86\release\in_svloopback.dll`
- `build\x86\release\loopback_probe.exe`
- `build\x86\release\endpoint_signal_probe.exe`
- `build\x86\release\plugin_abi_probe.exe`
- `build\x86\release\plugin_host_probe.exe`
- `build\x86\release\minidump_stack_probe.exe`
- `build\x86\release\rate_adapter_probe.exe`

Test native capture while audio is playing:

```powershell
.\build\x86\release\loopback_probe.exe 5
```

Check every active Windows playback endpoint for a live signal:

```powershell
.\build\x86\release\endpoint_signal_probe.exe 5
```

Validate the DLL ABI and exported Winamp entry point:

```powershell
.\build\x86\release\plugin_abi_probe.exe
```

Exercise the complete plug-in callback lifecycle through ten live start/stop
cycles in a small fake Winamp host:

```powershell
.\build\x86\release\plugin_host_probe.exe
```

Exercise the standalone rate-adapter utility across common and high sample
rates:

```powershell
.\build\x86\release\rate_adapter_probe.exe
```

The optional `install-local.ps1` helper performs a hash-verified copy and
refuses to overwrite a different existing DLL. Run it from an administrator
PowerShell after building.

## ABI provenance

The minimal interoperability declaration in `include/winamp_input_abi.h` was
checked against the Winamp 5.02 SDK input example (`in_raw/IN2.H`). No SDK
binary, source example, branding asset, preset, or Winamp implementation code
is included.

## AI assistance

This plug-in was created as a side project with OpenAI Codex assisting with
implementation, build automation, testing, and documentation.

## License and disclaimer

Released under the repository's [MIT License](../LICENSE). This independent
community project is not affiliated with or endorsed by Winamp.
