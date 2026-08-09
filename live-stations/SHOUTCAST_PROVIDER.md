# Adding SHOUTcast as a provider

SHOUTcast has its own official internet-radio directory. Some of its stations
also appear in Radio Browser, but the catalogs are maintained separately and
will not be identical.

Live Stations currently uses Radio Browser because its directory API is open
and designed for community clients. SHOUTcast is not enabled by scraping its
website or copying its catalog. An official integration should use the
SHOUTcast API under terms approved for this plug-in.

## What the project owner needs to do

1. Open the [SHOUTcast developer page](https://directory.shoutcast.com/Developer)
   and request API-partner access.
2. Describe the project as **Winamp Tools Live Stations**, a free, open-source
   hobby plug-in that restores internet-radio discovery inside Winamp.
3. Link to the public Winamp Tools repository.
4. Explain that the plug-in only helps Winamp play selected streams; it does
   not record, download, rebroadcast, or permanently copy the directory.
5. Ask SHOUTcast for its current API documentation, endpoints, rate limits,
   branding assets, and release-approval process.
6. Ask specifically how a Developer ID should be distributed in an open-source
   desktop application. Do not assume that it may be committed to GitHub or
   embedded openly in the DLL.
7. Ask whether its software-sample and marketing-approval requirements apply to
   this free community plug-in and, if so, what they expect.

Review the current
[SHOUTcast API license](https://www.shoutcast.com/legal/agreements/api) before
accepting it. The agreement currently includes registration, branding,
promotion, credential protection, terms-linking, and directory-use conditions.
The project owner—not Codex—must decide whether to accept that agreement.

## Questions to include in the request

```text
I maintain Winamp Tools, a free open-source hobby project that creates modern
plug-ins for classic 32-bit Winamp 5. I am building a native Media Library
station browser called Live Stations and would like to offer the official
SHOUTcast directory as an optional provider alongside the open Radio Browser
directory.

The plug-in will query current results, display required SHOUTcast attribution,
and hand a selected station URL to Winamp. It will not record or rebroadcast
content or permanently copy the SHOUTcast directory.

Please confirm:
- the current API documentation and endpoints;
- applicable rate limits;
- whether this open-source desktop use is approved;
- how the Developer ID should be protected and distributed;
- required branding and Terms-of-Use links;
- the marketing-review and software-sample process; and
- whether you require any additional agreement for a free community plug-in.
```

Never put an issued Developer ID, API key, email credential, or private partner
documentation in a public GitHub issue, source file, build log, or release.

## Planned technical integration after approval

The plug-in already has a common station model for name, stream URL, codec,
bitrate, country, tags, and source-specific identifiers. An approved SHOUTcast
provider can be added behind the same interface:

1. add **All**, **Radio Browser**, and **SHOUTcast** source choices;
2. query SHOUTcast live using its approved authentication method;
3. keep returned directory data in memory only for the current view;
4. label SHOUTcast results and display required branding and legal links;
5. merge **All** results without removing Radio Browser or experimental video;
6. avoid duplicate rows when both directories identify the same stream URL;
7. keep SHOUTcast radio separate from Radio Browser's experimental video list;
8. validate rate limits, errors, credential handling, playback, and cleanup
   before producing a release.

If SHOUTcast does not approve a distributable credential model, Live Stations
will continue working with Radio Browser. Users can also open direct station
URLs in Winamp without copying or impersonating the SHOUTcast directory.
