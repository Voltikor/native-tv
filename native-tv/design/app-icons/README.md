# Official app artwork

`official.json` pins developer-published Google Play images, their source URLs,
SHA-256 hashes and launcher aliases. Original square PNGs are under `official/`.
TV-specific listings are preferred; services without a separate TV listing use
the publisher's shared or standard Android icon. Exact adaptive composition on
a Google TV device has not been verified. TIDAL and RecordPlus publish JPEGs;
their PNG copies preserve the decoded pixels and record the source JPEG hashes.

The generator scales the complete square to 192 pixels without recolouring,
cropping the logo, replacing the background or adding a gradient. The existing
launcher applies its round mask and focus effects. No layout change is needed.
Transparency is preserved; the TV app's existing background shows through those
regions, so a store icon alone does not establish an adaptive icon's background.
Emby's complete icon is fitted to 76% of the square, with transparent padding,
to keep its ANDROID TV caption inside the circular mask. Its source is unchanged.
Apple TV has a baked-in glossy rounded-square border. Its generated tile trims
24 source pixels from each edge to remove the top/bottom bevel slivers under
the circular mask. The logo and original source file remain unchanged.

Coverage includes every video/music alias in `src/service/app_kinds.cpp`, plus
additional streaming services including Bandplay, Claro tv+, DAZN, Looke, MUBI,
Premiere, DGO, ViX, Viki and WeTV. Aliases for legacy names such as PlayPlus and
SBT Vídeos use their publisher's current RecordPlus and +SBT artwork.

The manifest explicitly lists exceptions: LG/webOS-only apps and clients retain
their own artwork or the coloured fallback. The standalone Telecine app was
retired; it keeps its own legacy art rather than using an unrelated Play listing.
The manifest links Globo's explanation of the migration to Globoplay.

The existing coloured design in `../app-icons.json` remains intact. Generate the
normal pack from the repository root:

```sh
uv run native-tv/tools/gen_app_icons.py
python3 native-tv/tests/app_icon_artwork.py
```

To reproduce the complete coloured backup without overwriting the normal pack:

```sh
uv run native-tv/tools/gen_app_icons.py --fallback-only --output /tmp/app-icons-coloured.pack
```

To update artwork, retrieve the original image from its official listing, verify
the publisher, replace the source image and its URL/hash in `official.json`, then
regenerate the pack and run the coverage check. Build and runtime need no artwork
downloads; the app uses the committed pack offline.

## Tile shapes and wide artwork

Quick settings → Home settings → App tile shape cycles Round, Square and Rectangle.
Round remains the default; the choice is stored in the active profile's layout.
Square tiles have rounded corners. Rectangles use a 16:9 frame and retain the
same height, with row spacing, scrolling and pointer targets using the wider size.

Banner source priority is the publisher's Android TV app resources first, then
verified publisher/Google-hosted TV artwork. Jellyfin and Kodi use their actual
Android TV resource PNGs, pinned to repository commits. Nine commercial-service
banners come from Google's published Google TV app sheet; these are marketing
references, not extracted APK resources. Their source rectangles and URLs are
recorded under `banners` in `official.json`. Those 11 banners are packed at
320×180 without stretching. The remaining 28 services use their complete square
icon centered in the wide frame. No square icon is cropped to fill a rectangle.

The best source for the remaining commercial banners is their original Android
TV package resource or a publisher-provided banner. We do not substitute fan-made
art or label mobile store feature graphics as TV launcher banners. No Google TV
device was available to verify the banners against its installed app versions.
