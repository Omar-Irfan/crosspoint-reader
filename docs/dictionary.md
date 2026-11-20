# Dictionary

Look up words and phrases while reading an EPUB using an offline StarDict dictionary stored on the SD card.

## Supported Format

The reader supports **StarDict** dictionaries. When searching for dictionaries online, look for "StarDict format" or files with `.dict`, `.idx`, and `.ifo` extensions.

A dictionary folder must contain:

- `.idx` — word index (required, **must be uncompressed** — a `.idx.gz` will not work; decompress it on your computer with `gzip -d` first)
- `.dict` or `.dict.dz` — definition data (`.dict.dz` is supported as-is; entries are decompressed on the fly during lookup)
- `.syn` — synonym index (optional; maps alternate spellings and irregular forms to their headword)
- `.ifo` — metadata (optional)

Not supported: dictionaries with 64-bit index offsets (`idxoffsetbits=64` in the `.ifo` — rare, and rejected with an error).

> [!NOTE]  
> If you use .dict.dz and Crosspoint failed to load your dictionary with error message "Not enough memory", try unzipping it.

## Setting Up a Dictionary

1. Copy your dictionary folder(s) to `/dictionaries/` on the SD card — one dictionary per folder, e.g. `/dictionaries/webster/webster.idx` + `webster.dict.dz`. A hidden `/.dictionaries/` folder (dot-prefixed) works the same way, for keeping it out of the file browser.
2. Open **Settings → Reader → Dictionary** on the device.
3. Select a dictionary from the list, or **None** to disable lookups.

The Dictionary setting only appears when at least one usable dictionary folder exists. Folders containing more than one dictionary (multiple `.idx` stems) are skipped as ambiguous.

## Looking Up Words and Phrases

Two ways to start a lookup while reading:

- Open the reader menu (**Confirm**) and choose **Look Up**, its first item.
- Or set **Settings → Controls → Front buttons → Menu long press** to "Dictionary", then hold **Confirm** (~0.4s) on the reading page.

### Selecting Text

**On button-driven devices:**

1. A selection cursor appears on a word. Use **Left/Right** to navigate between words in reading order, and the side **Up/Down** buttons to jump between lines or flip across nearby pages. Holding a button keeps moving.
2. Press **Confirm** (**Select**) to anchor the start of the selection.
3. **Single word:** Press **Confirm** again immediately (**Look Up**) to look up the highlighted word.
4. **Phrase / multiple words:** Use **Left/Right** or **Up/Down** to extend the highlighted selection across words, lines, or pages. Then press **Confirm** (**Look Up**).
5. Press **Back** to drop the anchor, or to cancel and return to reading.

**On touchscreen devices:**

- Tap or drag the selection handles across words to highlight a word or phrase, then tap **Look Up**.

On the very first lookup with a dictionary (and again whenever the `.idx` or `.syn` source file changes), the reader shows *"Indexing dictionary…"* while it builds small sidecar files next to them — a `.qidx` for the word index, and a `.sidx` when a `.syn` synonym file is present. Each sidecar is rebuilt independently, only when its own source changes. This takes a few seconds for large dictionaries and makes all subsequent lookups fast. The sidecars can be deleted safely at any time — they will simply be rebuilt.

### How Lookup Works

1. **Direct match** — the word or phrase is searched case-insensitively in the dictionary index. Leading and trailing punctuation and whitespace are stripped automatically. Hebrew niqqud and cantillation marks are removed from the search term so pointed text can match unpointed headwords. Basic Arabic vowel marks (U+064B–U+0650, U+0652) and superscript alef (U+0670) are also removed; shaddah, hamza, and maddah remain significant. Multi-word phrases match dictionary entries containing spaces (such as idioms, phrasal verbs, or compound terms). An index may list the same headword more than once (one entry per part of speech, for example); every entry it lists is shown, each labelled with its headword.
2. **Synonyms** — on a miss, if the dictionary ships a `.syn` file, alternate spellings and irregular forms recorded there are resolved to their headword (e.g. `oxen` → `ox`, `colour` → `color`). All the headwords a synonym points at are shown. This step is skipped if the `.sidx` sidecar could not be built (e.g. transient low memory during indexing); the dictionary otherwise stays usable, and the build is retried the next time it is opened.
3. **Stemming** — still no match: common English word forms are retried automatically: possessives and plurals (`dogs` → `dog`, `stories` → `story`) and verb endings (`walked` → `walk`, `running` → `run`, `making` → `make`).
   For Arabic, after the word itself is checked in the `.syn`, Arabic words get Arabic fallbacks instead of English stems: shadda and tatweel removed, hamza on alef folded (أ إ آ ٱ → ا), attached particles removed (و ف ب ك ل س, ال, لل), and pronoun suffixes removed (ـه ـها ـهم ـكم ـنا ـني …), restoring the spelling a suffix changed (مدرستهم → مدرسة, كتبوه → كتبوا). Forms with the fewest changes are tried first. Every form is tried as a headword before any form is tried in the `.syn`, so `بكتابه` finds `كتاب` rather than a related word. Arabic punctuation (، ؛ ؟ ٪ ۔ ﴾ ﴿) is trimmed from the selected word like Latin punctuation.
4. **Not found** — a short popup appears. Touchscreen devices return to reading; buttons-only devices return to word selection.

## Arabic Word Breakdown

On boards with PSRAM (for example X4 Pro and X4 Classic), an Arabic word's
definition starts with a line that splits it into prefixes, stem and suffixes,
with its root:

    وَ + بِ + كِتاب + هم  (ك ت ب)

When a word can be split two ways, both are shown (for example بسم). When the
definition shown is for another form of the selected text, or for a phrase,
the selected text is shown first. In a phrase gloss each word's root follows it.

The breakdown needs `/.crosspoint/arabic_morph.txt` on the SD card. Build it
from the Buckwalter Arabic Morphological Analyzer 1.0 data (GPL v2, (c) 2002
QAMUS LLC and the Trustees of the University of Pennsylvania; LDC catalogue
LDC2002L49, also bundled with GPL ports such as pyaramorph; the six files
`dictPrefixes`, `dictSuffixes`, `dictStems`, `tableAB`, `tableAC`, `tableBC`),
then copy the result to `/.crosspoint/arabic_morph.txt` on the SD card:

    python3 scripts/build_arabic_morph.py <data folder> arabic_morph.txt

The generated table is derived from that data and stays under GPL v2; it is not
part of the firmware. From the first Arabic lookup after starting, its tables use
about 210 KB of PSRAM for the session. Without the file, lookups work as before.
Roots that the data files list under the wrong heading are left out rather than
shown wrong.

## Phrase Gloss

When a selected phrase is not itself a dictionary entry, each word (up to 16) is looked up on its own. The definition screen lists the words in order, each followed by its dictionary form when that differs (`العابدين — عابد`) and the first lines of its definition. Lines that only name another form of a word (`plural of …`, `jussive of …`) are skipped when the entry has real meanings, and with the word breakdown the section of the entry matching the word's stem is used. With a merged dictionary, each word's lines start with the name of the source they came from (`[Wiktionary]`): the first source in the merged entry that has the word. A word with no entry is shown alone with a trailing dash. If no word is found, **Not found** appears. Glossed phrases are not saved to the vocabulary history. Selecting a phrase inside a definition glosses it the same way, and **Back** returns to the gloss.

## Ask AI

An optional on-demand explanation from the Claude or Gemini API, for when the dictionary entry is not enough. It is off unless the SD card holds `/.crosspoint/ai.json`:

```json
{"api_key": "sk-ant-...", "provider": "anthropic", "model": "claude-opus-5-5", "explain": "arabic"}
```

Only `api_key` is required.

- `provider`: `anthropic` (default) or `gemini`. With Gemini, use a Google AI Studio key.
- `model`: defaults to `claude-opus-5-5` for Anthropic and `gemini-3.8-flash` for Gemini. Any model name the provider accepts works; check the provider's current model list and prices.
- `explain`: `arabic` (default) or `english`, the language of the first explanation.

The reply is laid out like a matn with its sharh underneath. **المتن** (Text) is the sentence around your selection (up to about 30 words either side, within the paragraph), copied from the book; the model never rewrites it. Without it (low memory, or a word looked up inside a definition) the matn is the selected text. Under it comes **الشرح**, the point of the passage and any hard words, or in English a literal-leaning **Translation** and then an **Explanation**. Answers assume a reader who knows the religion's basics, skip i'rab, and write honorifics as ﷺ and ﷻ.

With the file present, the definition screen's Confirm hint reads **Look/AI**: press Confirm to select words as usual, or **hold Confirm** to ask about the word or phrase you looked up. On touch devices you can also tap **Ask AI** at the right end of the definition's header. The reader joins Wi-Fi (showing the network list if not connected) and sends the selected text to the chosen provider's API (Anthropic or Google), together with the sentence it came from (not for words looked up from inside a definition). Nothing is sent unless you ask.

The answer screen offers the other language: press Confirm, or tap **English** / **Arabic** at the right end of the header on touch devices, to ask the same question again in that language. The last answers fetched (six, or two on devices without PSRAM) are kept while the definition is open, so switching back, or asking about the same text again, shows them without a new request; they are discarded when you return to the book.

Wi-Fi stays on while the definition is open, so further questions are quick. When you leave the definition the reader turns Wi-Fi off and briefly restarts into your book, as after any other Wi-Fi feature, to free the memory the connection used.

- Each request is billed to the key's account; check the provider's terms for how prompts are used, especially on free tiers. Use a key with a spending limit: anyone with the SD card can read it. The web file manager never serves or replaces `ai.json`, so copy it to the card directly.
- Both services' connections are checked against the Google Trust Services and GlobalSign root certificates, so the clock must be set. The reader syncs it over Wi-Fi when needed and shows **Clock not set** if it cannot.
- **AI key rejected**: the API refused the key (HTTP 401/403, or Gemini's invalid-key error). **AI declined**: the model refused to answer. **No AI key**: the config file is missing or has no key. **Not Enough Memory**: too little free memory for the secure connection. **AI request failed**: Wi-Fi was cancelled or could not connect, or another network or response error.
- After adding the file for the first time, restart the reader (or let it sleep and wake) so the **Look/AI** hint appears. Once enabled, the file is read again for each request.

## Vocabulary History

Successful lookups started from a book page are appended to `/Vocabulary.txt` at the root of the SD card. This is separate from `My Clippings.txt`; looking up a word does not create a highlight. Failed lookups are not recorded, and an SD-card write failure does not interrupt the dictionary.

The file uses UTF-8 **JSON Lines**: one JSON object per line, versioned with `"v":1`. Each record has a random `id`, `word` (selected text), `headword` (dictionary match), `definition` (plain text), and `timestamp` (Unix seconds, or `null` when the clock is unavailable). It also includes `title`, `author`, `series`, `series_index`, `chapter`, `page` (1-based chapter page), `spine` (0-based EPUB spine index), and `start_offset` (EPUB source offset, or `null` when unavailable). Missing series information is represented by an empty name and a `null` index. Definitions are limited to 2 KiB, looked-up words and matched headwords to 160 UTF-8 bytes each, and book fields to bounded lengths. The `definition_truncated` boolean indicates whether the definition (but not the word or headword) was shortened; when possible the saved definition ends at a complete line. HTML dictionary definitions are converted to plain text.

Repeated lookups are recorded separately. Positions reflect the reader layout at lookup time and may change when the book is reflowed. The `chapter` title is resolved from the selected word's rendered page, not its exact offset: if two TOC chapter boundaries share a page, the title may refer to the later chapter rather than the one containing the word. Lookups made from inside a dictionary definition are not currently included.

## The Definition Screen

When a word is found, the definition screen shows the matched headword at the top and the definition text below, with a page counter for long definitions. A word that maps to several definitions shows them one after another, each labelled with its headword. Definitions that exceed the remaining size limit are skipped, and later definitions are still considered. If no labelled definition fits, the first definition is shown alone without an extra label.

HTML dictionaries (`sametypesequence=h`) and XDXF dictionaries (`sametypesequence=x`) use the EPUB text-layout engine for supported formatting such as headings, bold, italics, lists, and line breaks. Unrecognized tags keep their text without special formatting. Images and CSS styling are ignored. Definitions that are too large or cannot be parsed or laid out within the available memory fall back to plain text.

A readable `.ifo` file with a missing or empty `sametypesequence` enables the type stored in each entry field. HTML (`h`) and XDXF (`x`) fields use the same formatting support. Text fields are shown as text; binary fields, such as images and sounds, are skipped. Missing, empty, or unreadable `.ifo` files retain plain-text handling.

Inline entries with only empty or binary fields are skipped. If all matching entries are empty, the lookup reports no match.

Definitions are drawn in your reading font. Dictionaries may use IPA symbols (e.g. `ˈ ː ɹ ɪ ʊ`…) as pronunciation guide, which built-in reader fonts do not include, so they are shown as `�`. To see them, install an [SD card font](./sd-card-fonts.md) that covers IPA and select it as the reading font.

- **Left/Right** or side **Up/Down** — previous / next page
- **Confirm** — select words or a phrase inside the definition, then choose **Look Up**
- **Long press on text** — select the touched word on touchscreen devices when touch reader controls are enabled
- **Back during selection** — cancel selection
- **Back while reading a definition** — return to the previous definition. At the first definition, touchscreen devices return to reading; buttons-only devices return to book word selection
- **Long Back while reading a definition** — hold **Back** for at least one second, then release to close all definitions and return directly to reading
- **Long press the header Back arrow** — close all definitions and return directly to reading on touchscreen devices, even when touch reader controls are disabled

During definition word selection, press **Back** to cancel selection first, then hold **Back** or long-press the header Back arrow to leave the lookup chain.

Nested lookups use the same dictionary. Both formatted and plain-text definitions support selection.
Returning to a previous definition restores its page and remembers its selection for the next press of **Confirm**.

The reader keeps up to 16 previous definitions. At this limit, another lookup shows a brief message and leaves the current definition open.
A failed lookup reloads the current definition and keeps its page and selection.

The current definition is released before loading another entry, so two definitions do not need to fit in memory together.
Returning to a previous definition reads it from the SD card again.
If a previous definition cannot be read or its position restored, the definition screen closes. Touchscreen devices return to reading; buttons-only devices return to book word selection.
The screen also closes if a failed forward lookup cannot reload the current definition.


## Where to find dictionaries

> credit to https://github.com/koreader/koreader/wiki/Dictionary-support for the list.

- The [reader.dict](https://www.reader-dict.com) (ex "BoboTiG/ebook-reader-dict") project provides StarDict version of daily dumps of [Wiktionary](https://www.wiktionary.org/) monolingual dictionaries for a variety of languages. It also provides [non-free multilingual](https://www.reader-dict.com) dictionaries.
- The [WikDict](https://www.wikdict.com) project provides free bilingual dictionaries based on [Wiktionary](https://www.wiktionary.org/) for a lot of language pairs. StarDict versions can be [downloaded from here](https://download.wikdict.com/dictionaries/stardict/).
- The [`Vuizur/Wiktionary-Dictionaries`](https://github.com/Vuizur/Wiktionary-Dictionaries) repository contains dictionaries based on [Wiktionary](https://www.wiktionary.org/) from many languages to English, including English-English.
- The [DictInfo](https://www.dictinfo.com/) website provides outdated monolingual dictionaries based on [Wiktionary](https://www.wiktionary.org/).
- The [Firedict site](https://tuxor1337.frama.io/firedict/dictionaries.html) contains a list of freely available dictionaries.
- [wiktionary_stardict](https://xxyzz.github.io/wiktionary_stardict/): update monthly.
- [Fictionaries](https://fictionary.gumroad.com/) provides dictionaries for various speculative fiction books and series.
- [World Factbooks Archive](https://github.com/MilkMp/CIA-World-Factbooks-Archive-1990-2025) provides 36 years of CIA's World Factbook dictionaries in StarDict format.
- [StarDict-Hebrew](https://github.com/Uri-Tauber/StarDict-Hebrew) Hebrew-English StarDict versions of Babylon dictionaries.
