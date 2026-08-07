The first Winamp Tools release.

`in_svloopback.dll` lets AVS, MilkDrop, and other Winamp visualizers react to
audio playing through the current default Windows output device. It uses WASAPI
loopback directly, so Stereo Mix, monitoring, virtual cables, and software
mixers are not required.

### Quick use

1. Download and unzip `winamp-wasapi-loopback-v0.1.0-x86.zip`.
2. Close Winamp and copy `in_svloopback.dll` to its `Plugins` folder.
3. Restart Winamp, press Ctrl+L, and open `loopback://`.
4. Start AVS, MilkDrop, or another visualizer.

This is an experimental, unsigned 32-bit Winamp 5 plug-in for Windows 10/11.
It was tested with Winamp 5.92, AVS, MilkDrop 2, and a Bluetooth output device.
Source device rates from 8 kHz through 384 kHz are adapted to a steady 48 kHz
visualization stream so high-rate devices do not flood legacy plug-ins.
Normal Windows PCM 8/16/24/32-bit and floating-point 32/64-bit output mixes are
accepted. Original media codecs and compressed bitrates are already decoded by
the source application before loopback capture.

SHA-256: `75DA6BBF5EBD6F8F50669511BB2753411F08B2E00C3DAF9E171095873F010B8E`

Made as a hobby project by a Winamp fan, with OpenAI Codex assisting with the
implementation, testing, packaging, and documentation.
