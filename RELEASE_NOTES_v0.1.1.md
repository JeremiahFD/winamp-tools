# Winamp WASAPI Loopback Input v0.1.1

The original v0.1.0 download did not work reliably in real Winamp testing. It
could crash Winamp while starting or stopping `loopback://`, or leave the
visualizer black. Do not use v0.1.0. This v0.1.1 replacement corrects the
cleanup and live-input behavior and should fix those failures.

## Fixed

- Releases every WASAPI COM interface before uninitializing the capture
  thread's COM apartment. The previous cleanup order could corrupt Winamp's
  process heap inside `combase.dll` / `ntdll.dll`.
- Adds a live fake-host integration probe that exercises ten complete
  `Play`/`Stop` cycles, verifies Winamp's visualization callback arguments,
  receives PCM data, and confirms balanced visualization initialization and
  cleanup.
- Adds a crash-dump stack probe used to trace the original failure to the
  plug-in's COM cleanup path.
- Expands the main README with beginner-friendly installation and usage steps,
  including exactly where to enter `loopback://`.
- Adds an endpoint signal probe that identifies which active Windows playback
  device is actually carrying audio.

## Verified

- The exact release DLL was confirmed in Winamp 5.92 with live Realtek output
  driving the visualizer.
- The release DLL passed its x86 Winamp ABI check and ten complete live
  start/stop cycles with balanced cleanup and PCM callbacks.
- Normal PCM 8/16/24/32-bit and floating-point 32/64-bit Windows mix formats
  are supported. The native Windows device mix rate is passed to Winamp;
  automated rate handling checks cover 8 kHz through 384 kHz.

## Important

Use `loopback://`, not `linein://`. The latter starts Winamp's separate legacy
Line Input plug-in and is unrelated to this project.

This remains an experimental, unsigned x86 input plug-in for 32-bit Winamp 5
on Windows 10/11. Some protected or exclusive-mode audio may not be available
to Windows loopback capture, and unstable visualization presets can still
crash Winamp independently.

Verified DLL SHA-256:

`9D8B9A8F9DEE66D3001D1E39042B8B0670C47FF6D4301DEDB6B4B2DF0EABE45A`

Verified ZIP SHA-256:

`005F03D443409623BAEA10F8E56E940684F1C65ABBFC818198A570F9315B593E`
