Winamp WASAPI Loopback Input v0.1.1 (x86)

Lets Winamp visualizers react to audio playing through the current default
Windows output device. No Stereo Mix, monitoring, virtual cable, or mixer is
required.

INSTALL
1. Close Winamp.
2. Copy in_svloopback.dll to C:\Program Files (x86)\Winamp\Plugins
3. Restart Winamp.
4. Press Ctrl+L, enter loopback://, and press Enter.
5. Start AVS, MilkDrop, or another visualizer.

After changing the default Windows output device, stop and reopen loopback://.

This is an experimental, unsigned 32-bit Winamp 5 plug-in for Windows 10/11.
Some protected or exclusive-mode audio may not be captured. Old AVS/MilkDrop
presets can still have their own compatibility problems.

The Windows output device's native mix rate is passed to Winamp instead of
assuming 44.1 or 48 kHz. Automated rate handling checks cover 8 kHz through
384 kHz, though individual legacy visualizers may have narrower limits.
Normal Windows PCM 8/16/24/32-bit and floating-point 32/64-bit output mixes are
supported. Original codecs and compressed bitrates are decoded before capture.

The original v0.1.0 download did not work reliably in real Winamp testing and
could crash or leave the visualizer black. Do not use v0.1.0. This v0.1.1 build
corrects the WASAPI/COM cleanup and live-input behavior and should fix that
failure. It was verified in Winamp 5.92 with live Realtek output.

SHA-256
9D8B9A8F9DEE66D3001D1E39042B8B0670C47FF6D4301DEDB6B4B2DF0EABE45A

Source and updates:
https://github.com/JeremiahFD/winamp-tools

Created as a side project with OpenAI Codex assisting with implementation,
testing, packaging, and documentation.

Not affiliated with or endorsed by Winamp.
