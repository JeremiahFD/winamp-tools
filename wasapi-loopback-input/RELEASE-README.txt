Winamp WASAPI Loopback Input v0.1.0 (x86)

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

Source device rates from 8 kHz through 384 kHz are adapted to a steady 48 kHz
visualization stream for compatibility with legacy visualizers.
Normal Windows PCM 8/16/24/32-bit and floating-point 32/64-bit output mixes are
supported. Original codecs and compressed bitrates are decoded before capture.

SHA-256
75DA6BBF5EBD6F8F50669511BB2753411F08B2E00C3DAF9E171095873F010B8E

Source and updates:
https://github.com/JeremiahFD/winamp-tools

Created as a side project with OpenAI Codex assisting with implementation,
testing, packaging, and documentation.

Not affiliated with or endorsed by Winamp.
