# Beginner Guide: Make Winamp Visualizers React to Your Computer's Audio

This guide is for Windows users who just want MilkDrop, AVS, or another Winamp
visualizer to react to music or video playing on their computer.

You do **not** need to install a virtual cable, Stereo Mix, a recording device,
or a software mixer.

## Before you start

You need:

- a Windows 10 or Windows 11 computer;
- the regular 32-bit Winamp 5 application already installed;
- an installed Winamp visualizer such as MilkDrop or AVS; and
- another program that can play sound, such as a web browser, Spotify, VLC, or
  a second copy of Winamp.

This plug-in does not play music by itself. It gives Winamp's visualizer a copy
of sound that is already playing elsewhere on your computer.

## Three words you will see

**ZIP file**: a downloaded folder that needs to be opened or extracted.

**DLL file**: the small plug-in file that Winamp needs. Do not double-click it.
You copy it into Winamp's `Plugins` folder.

**`loopback://`**: a short instruction for Winamp. You type it into Winamp's
own **Open URL** box. It is not a website, file name, or browser address.

## Step 1: Download the correct file

1. Open the [latest release page](https://github.com/JeremiahFD/winamp-tools/releases/latest).
2. Under **Assets**, click `winamp-wasapi-loopback-v0.1.1-x86.zip`.
3. Wait for the download to finish. It is normally in your **Downloads**
   folder.

Do not download the older `v0.1.0` file. It is marked superseded because it
did not work reliably.

## Step 2: Open the ZIP file

1. Open File Explorer. You can press the Windows key and type **File Explorer**.
2. Open **Downloads** on the left side.
3. Double-click `winamp-wasapi-loopback-v0.1.1-x86.zip`.
4. You should see three files. The one Winamp needs is named:

   ```text
   in_svloopback.dll
   ```

If Windows shows **Extract all** at the top, you can click it first, choose a
folder, and click **Extract**. That makes copying the DLL easier.

## Step 3: Put the DLL into Winamp

1. Completely close Winamp.
2. Open another File Explorer window.
3. Click the address bar at the top, paste this exact folder path, and press
   Enter:

   ```text
   C:\Program Files (x86)\Winamp\Plugins
   ```

4. Go back to the ZIP or extracted folder.
5. Drag `in_svloopback.dll` into the Winamp `Plugins` folder, or right-click it
   and choose **Copy**, then right-click an empty part of the `Plugins` folder
   and choose **Paste**.
6. If Windows asks for administrator permission, click **Continue**.
7. If Windows says a file with that name already exists, choose **Replace the
   file in the destination**. That updates an older copy of this plug-in.

If that folder path does not exist, Winamp was installed somewhere else. Find
your Winamp shortcut, right-click it, choose **Open file location**, then open
the `Plugins` folder beside `winamp.exe`.

## Step 4: Tell Winamp to listen to your computer's sound

1. Start Winamp again.
2. Press and hold the **Ctrl** key, then press the **L** key once: **Ctrl+L**.
3. A small Winamp window called **Open URL** or **Play URL** should appear.
4. Click in its text box and type exactly:

   ```text
   loopback://
   ```

5. Click **Open**, or press Enter.

The spelling matters. Do **not** type `linein://`. That starts a different,
older Winamp plug-in and is not this download.

When it worked, Winamp should show **System Output (WASAPI Loopback)** in its
title or playlist.

## Step 5: Play sound somewhere else

Start music or video in a browser, Spotify, VLC, a game, or another audio app.
You should hear it normally through your speakers or headphones.

Important: while `loopback://` is open, this same Winamp window is listening to
the computer's sound. It is not also playing a song from its Media Library.
Use another app for the music or video.

## Step 6: Start the visualizer

1. In Winamp, open **Options** > **Visualization** > **Select plug-in**.
2. Choose MilkDrop, AVS, or another visualizer you already installed.
3. Start the selected visualizer. In many Winamp layouts, **Ctrl+Shift+K**
   starts or stops it.

If you use MilkDrop and see a black window, click inside that window and press
**L** to open its preset list. Try one of MilkDrop's built-in presets first.

## If the visualizer stays black or still

1. Make sure sound is actually playing and you can hear it.
2. Check that Winamp says **System Output (WASAPI Loopback)**, not a song name.
3. If you changed from speakers to Bluetooth, HDMI, headphones, or another
   output device, stop `loopback://` and repeat Step 4. The plug-in connects
   to the output device that was default when it started.
4. Use a built-in MilkDrop or AVS preset first. Some old downloaded presets can
   crash or fail independently of this plug-in.
5. Do not use `linein://` for this plug-in.

## Remove it later, if you want

Close Winamp, open its `Plugins` folder again, and delete
`in_svloopback.dll`. This does not remove your music, playlists, or visualizer
presets.

## Still stuck?

Open a [GitHub issue](https://github.com/JeremiahFD/winamp-tools/issues) and
include your Windows version, Winamp version, what audio app you were using,
and whether the sound was coming through speakers, Bluetooth, HDMI, or
headphones.
