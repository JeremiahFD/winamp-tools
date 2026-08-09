# Winamp Tools Live Stations

An experimental 32-bit Winamp Media Library plug-in that provides a current,
native station browser instead of depending on Winamp's broken legacy Online
Services web page.

The plug-in is in development and has **not been released yet**. Its isolated
host and live-directory checks pass, but it still needs testing inside a real
Winamp installation before a public download is prepared.

## What it does

- adds **Live Stations** to the Winamp Media Library tree;
- searches the community-maintained [Radio Browser](https://www.radio-browser.info/)
  directory at runtime;
- discovers Radio Browser API mirrors through DNS and tries another mirror if
  one is unavailable;
- hides stations the directory currently marks as broken;
- shows station name, codec, bitrate, country, and tags in a native Windows
  list rather than an embedded web page;
- resolves the selected station through Radio Browser's click endpoint and
  sends the resulting HTTP(S) stream URL to Winamp;
- leaves the existing Winamp playlist intact by adding the station and playing
  that new final item;
- offers a separate **Video (experimental)** view for entries labeled H.264,
  MP4, or FLV by the directory.

The plug-in is a station browser, not a decoder. Audio or video playback still
depends on the input plug-ins installed in Winamp. Common MP3 and AAC radio
streams are the practical first target. Modern HLS video streams may not play
in a stock Winamp installation even when the directory reports them as live.

## Installation and use

See the [step-by-step Live Stations guide](../LIVE_STATIONS_GUIDE.md). It
explains where the DLL goes, how to open Winamp's Media Library, where the new
item appears, and what to try if a station does not play.

## Why Radio Browser is the primary source

[Radio Browser's API](https://api.radio-browser.info/) is open, documents
mirror discovery and failover, provides health metadata, and permits compatible
clients to use the directory. The plug-in identifies itself with a descriptive
user agent and uses the documented click endpoint when a station is selected.
It does not ship a frozen copy of the station catalog.

SHOUTcast still operates a directory and developer program, but its current
[API license](https://www.shoutcast.com/legal/agreements/api) requires
registration and includes additional branding, approval, storage, and usage
conditions. It is therefore not silently bundled as a provider. It could be
added later as an optional integration after obtaining the required approval.
See [Adding SHOUTcast as a provider](SHOUTCAST_PROVIDER.md) for the application
questions, credential boundary, and planned integration.

RadioFeeds remains useful for UK and Ireland stations, but its Winamp page is
currently HTTP-only and its HTTPS certificate does not validate for that host.
It is not embedded in this first plug-in.

## Safety boundaries

- API traffic uses HTTPS.
- Only selected `http://` or `https://` stream URLs are passed to Winamp.
- Control characters, oversized URLs, localhost, `.local`, loopback,
  link-local, and private numeric IPv4 targets are rejected.
- Network work runs outside Winamp's UI thread and has finite timeouts.
- Response size is capped before XML parsing.
- The plug-in does not record, relay, or host station content.

Directory entries are supplied by third parties. A successful directory health
check does not guarantee that a stream is legal in every location, continuously
available, or compatible with a particular Winamp setup.

## Build and checks

Requirements:

- Visual Studio Build Tools with the x86 C++ compiler;
- Windows 10/11 SDK;
- PowerShell.

From this directory:

```powershell
.\build.ps1 -Configuration Release
```

Outputs are written to `build\x86\release`:

- `ml_livestations.dll`
- `radio_browser_probe.exe`
- `plugin_abi_probe.exe`
- `plugin_host_probe.exe`

Run the automated checks:

```powershell
.\build\x86\release\plugin_abi_probe.exe .\build\x86\release\ml_livestations.dll
.\build\x86\release\radio_browser_probe.exe
.\build\x86\release\radio_browser_probe.exe video
.\build\x86\release\plugin_host_probe.exe .\build\x86\release\ml_livestations.dll
```

The host probe loads the DLL into a small fake Media Library host, waits for the
native radio and experimental-video lists, selects a result, resolves its URL,
verifies the Winamp enqueue/play messages, and unloads the DLL. It also destroys
a second view while its startup request is beginning to exercise cancellation
and cleanup. It does not prove that a real Winamp decoder can play every
returned stream.

## ABI provenance

The minimal interoperability declaration in
`include/winamp_media_library_abi.h` was derived from the Winamp community
source `ml.h` and `wa_ipc.h`. Its original permissive notice is retained in
that header. No Winamp binary, UI asset, service implementation, or station
catalog is included.

The remaining project code is released under the repository's MIT License.

## AI assistance and disclaimer

This hobby plug-in was created with OpenAI Codex assisting with research,
implementation, testing, and documentation. It is an independent community
project and is not affiliated with or endorsed by Winamp, SHOUTcast, or Radio
Browser.
