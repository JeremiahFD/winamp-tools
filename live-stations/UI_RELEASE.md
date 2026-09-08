# Live Stations v0.1.0-beta.1 — radio UI

Browse internet radio inside Winamp's Media Library. Windows and 32-bit Winamp
are required; this beta has been tried with Winamp 5.9.2.

- Switch between Browse and Stations; open saved Favorites from the top bar.
- Filter by location, genre, language, codec, bitrate and compatibility.
- Resize panes and table columns, sort columns, and show/hide filters or details.
- Browse bounded pages of 50, 100 or 200 stations. Provider totals and the number
  shown on the current page are separate; counts change as the directory changes.
- Layout adapts to available space, with scrolling for small windows and details
  beside or below the list. Native controls use available Media Library colors.

## Install or update

1. Close Winamp normally. Keep a backup of any existing `ml_livestations.dll`.
2. Extract the ZIP and copy `Plugins/ml_livestations.dll` into the real Winamp
   installation's `Plugins` folder, not the Start Menu shortcut folder.
3. Reopen Winamp and choose **Media Library > Live Stations**.

No library, favorites, settings or other plug-ins need to be replaced. To roll
back, close Winamp and restore your previous DLL (or remove this DLL).

This is **beta** software and may be incomplete or incorrect. Skin details,
scaling, streams and decoder compatibility can vary. Radio Browser is an external
directory; station links can change or stop working. The plug-in connects to the
directory and selected broadcasters, not an audio hosting service.

Video, the playlist-resolver companion and chat are excluded. There is no bundled
Winamp installer. Original project code is MIT-licensed; see `LICENSE` and
`THIRD_PARTY_NOTICES.txt`. Winamp and station content are not covered by our MIT
license. This project is independent of Winamp and Radio Browser.
