# Native launcher translations

The launcher has its own **Language** picker, reached through Quick settings → Full settings → Settings, or
Settings. **Automatic — follow TV** reads `localeInfo.locales.UI` from the TV's
preferences at launch. English and Português (Brasil) can be selected explicitly.
The daemon saves the choice atomically as `language` in `home.json` through the
existing layout API; the jailed app never writes server files. The choice is
global across profiles; saved profile layouts do not carry it. It does not change the TV's system language or the
browser dashboard's language. Switching rebuilds the picker, refreshes API
reads and preserves the Home app selection. It sends no Home takeover command.
The launcher also accepts `--lang en` or `--lang pt-BR` for a temporary test run.

Native text uses the same `say::msg("key", "English", variables)` convention.
`LocaleScope` supplies a client-owned context on the UI thread: it resolves the
key directly, without looking up the English output. HTTP workers have no such
scope. Existing daemon messages stay English internally and are translated for
each request through `X-Native-Tv-Lang`, as before. The native catalog lives in
`native-tv/assets/i18n/launcher/pt-BR.json`; it also covers server display messages.
English is the fallback if an entry is missing, stale or unusable. Unsupported
system languages fall back to English; generic `pt` resolves to `pt-BR`, while
`pt-PT` remains English until that variant has its own catalog.

Whole-message placeholders work as in [Strings](strings.md). Integer counts use
`localizedPlural("key", "{count} item", "{count} items", count)`, with an entry
whose `from` contains the English `one` and `other` forms, and whose `text`
contains CLDR `one`, `many` and `other` forms for Brazilian Portuguese. English
uses `one` only for 1; Brazilian Portuguese uses it for 0 and 1, and `many` for
nonzero multiples of a million. Fractional and ordinal counts are not supported.
Display-only decimals and grouped integers use the shared locale formatters.
Raw numbers, API values, identifiers and user-chosen names are never localized.
Settings toggles use explicit boolean state, rather than recognizing display text.

TMDB requests use `pt-BR` or `en-US` for title metadata. Details and search caches
include the language; discovery keeps independent English and Portuguese caches,
so a browser using English does not replace the launcher's Portuguese feed.
Portuguese title details also request English trailers when available. TMDB's
own translation coverage still determines which titles and descriptions it returns.

Run `python3 scripts/check-launcher-strings.py` alongside `check-strings.py`.
It requires complete native Portuguese coverage and checks the source English,
placeholders, plural forms, Unicode normalization and bundled font coverage.
`--template` extracts the native messages. A new language also needs locale
registration, its plural/formatting rules, a picker entry and packaging entries.
The current renderer and font subset support Latin text; additional scripts and
right-to-left layout require separate rendering work.

Tests `native-locale` and `native-locale-ui` cover fallback, plurals, thread
isolation, server-backed choice, live language switching and unchanged control commands
and settings states. Content and HTTP tests cover cache isolation and language
headers. Test Portuguese layouts on a TV before release: longer labels may need
wording adjustments even when the font covers every character.
