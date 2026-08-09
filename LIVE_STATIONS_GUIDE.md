# Live Stations: step-by-step guide

This guide is for the upcoming **Winamp Tools Live Stations** plug-in. There is
no public download yet because the first build still needs real Winamp testing.

Unlike the WASAPI visualizer plug-in, you do **not** type `loopback://`,
`linein://`, or any other address into Winamp. Live Stations appears as a new
page inside the Media Library.

## Install the DLL

1. Download the Live Stations ZIP from the Winamp Tools Releases page after a
   tested release is published.
2. Close Winamp completely.
3. Open the ZIP and find `ml_livestations.dll`.
4. Find the folder where Winamp is installed. The usual location for classic
   32-bit Winamp is:

   ```text
   C:\Program Files (x86)\Winamp
   ```

   If Winamp is elsewhere, right-click its shortcut and choose **Open file
   location**.
5. Open the `Plugins` folder inside the Winamp folder.
6. Copy `ml_livestations.dll` into that `Plugins` folder. Windows may ask for
   administrator permission when Winamp is under Program Files.
7. Start Winamp again.

## Find and play a station

1. Open Winamp's **Media Library**. In classic Winamp, press **Alt+L** or use
   the **ML** button if your skin shows one.
2. Look down the list on the left and click **Live Stations**.
3. Wait for the station list to load. The status line reports how many stations
   were returned and which Radio Browser mirror answered.
4. To narrow the list, type a station or network name in the search box and
   click **Search**.
5. Click a station once, then click **Play**. You can also double-click it.
6. The station is added to the end of Winamp's playlist and selected for
   playback. Existing playlist entries are not erased.

## Experimental video entries

Choose **Video (experimental)** from the menu beside the search box. These
entries are separated from radio because finding a live URL and decoding it are
two different things.

The directory currently includes H.264, MP4, and FLV-labeled streams, many of
which use modern HLS playlists. Stock Winamp may not have an input plug-in that
can decode a particular stream. A video appearing in the list therefore does
not promise that it will play. Do not install random decoder DLLs from an
unknown download site to make one station work.

## If Live Stations does not appear

1. Confirm the DLL is named `ml_livestations.dll`.
2. Confirm it is directly inside Winamp's `Plugins` folder, not still inside
   the ZIP or another subfolder.
3. Confirm you restarted Winamp after copying it.
4. Open **Preferences > Plug-ins > Media Library** and look for
   **Winamp Tools Live Stations**.
5. This first build is for 32-bit Winamp 5 on Windows. A 64-bit player cannot
   load this x86 DLL.

## If one station does not play

Try two or three other stations, preferably MP3 or AAC entries. Individual
stations change addresses, go offline, block some countries, or use codecs that
the current Winamp installation cannot decode. The plug-in asks the directory
for stations marked healthy, but no public station directory can guarantee
continuous playback.

If every station fails, check whether Winamp itself can open a normal direct
radio URL and whether a firewall or security product is blocking Winamp.

## What this replaces—and what it does not

The old **Online Services** page can show HTTP 500 because its remote Winamp
service is failing. Live Stations adds a separate native page and does not
repair or modify that legacy page.

The plug-in also does not rebroadcast, save, or record streams. It finds current
station metadata and asks Winamp to play the selected public URL.
