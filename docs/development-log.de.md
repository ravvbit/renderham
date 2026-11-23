<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see ../README.md.
-->

> **Historical development log (German).** This file is the project's original README, which
> grew into a diary of the AI-assisted development: design decisions, how each feature was
> verified, bugs found and fixed. It is kept for transparency and as background reading.
> It is **not maintained** any more and parts are outdated (for example defaults that have
> changed since); the current documentation is the [README](../README.md).
> Paths such as `aic_cli` and the name "Amiga Image Converter" refer to earlier states of the
> project. "Sandbox" in the text means the AI's isolated, headless build-and-test environment
> (Xvfb, simulated mouse and keyboard), which explains the remarks about what could not be
> tested interactively.

# renderHAM

Konvertiert beliebige Fotos in HAM6-kodierte IFF-ILBM-Dateien, die von
jedem Standard-Amiga-Betrachter (Deluxe Paint, Multiview, ...) auf
OCS-Hardware nativ dargestellt werden können. Quantisierung, Dithering
und HAM6-Encoding laufen dabei als ein einziger kombinierter Algorithmus
(siehe `renderhamlib/include/aic/ham6_encoder.hpp` für die Begründung).

Dies ist **Ausbaustufe 1**: PC-seitiger Konverter, klassisches
Voll-Bild-HAM6 (keine Sliced-HAM/Dynamic-HiRes-Copper-Tricks, kein
Amiga-seitiger Player nötig, da Standard-ILBM/HAM von existierender
Amiga-Software gelesen wird).

## Architektur

```
renderhamlib/            statische Bibliothek – die gesamte Funktionalität
  include/aic/      öffentliche Header (Interfaces + Datentypen)
  src/               Implementierung

tools/renderham/      CLI-Frontend, nutzt ausschließlich die öffentliche
                     renderhamlib-API (main.cpp kennt keine Konvertierungs-
                     details).

tools/renderhamgui/      wxWidgets-GUI, nutzt dieselbe renderhamlib-API 1:1 -
                     siehe eigener Abschnitt weiter unten.

cmake/              wiederverwendbare CMake-Bausteine (Whole-Archive-
                     Linking, siehe Modularität weiter unten).

third_party/stb/    vendored Header-only-Libs (stb_image, stb_image_write,
                     stb_image_resize2) – kein Internetzugriff beim Bauen nötig.
```

**Pipeline** (`aic::ConversionPipeline::convert`):

```
Image::load → [crop_image] → resize_image → HAM6Encoder::encode → write_ham6_ilbm
 (stb_image)   (optional,      (Scale onto      (Quantizer + Ditherer   (IFF ILBM,
                pixel/percent   target canvas:    kombiniert, s.u.)      ByteRun1-RLE,
                WxH+X+Y)        Fit/Fill/Stretch,                        CAMG-HAM-Flag)
                                linear-light)
```

### Warum Quantisierung + Dithering + HAM-Encoding EIN Schritt sind

HAM6 ist eine 12-Bit-RGB-Repräsentation mit Delta-Kompression: jedes
Pixel ist entweder eine Palettenauswahl (Mode 00) oder verändert genau
einen Kanal (R/G/B) des tatsächlich angezeigten Vorgänger-Pixels
(Mode 01/10/11). Welche Farben von Pixel N aus "billig" erreichbar
sind, hängt also vom real gewählten Vorgänger ab – nicht von einer
abstrakten Zielpalette. `HAM6Encoder::encode()` durchläuft deshalb jede
Scanline links→rechts und bewertet pro Pixel alle 64 erreichbaren Codes
(16 Paletten- + 48 Delta-Kandidaten) direkt gegen den (dither-)
verschobenen Zielwert in OKLab – Quantisierung, Dithering-Bias und
HAM-Entscheidung sind dadurch ein einziger Vergleich, kein
Drei-Phasen-Prozess.

### Modularität / Erweiterbarkeit

Neue Dithering- oder Palettenauswahl-Algorithmen brauchen **keine**
Änderung an Pipeline, CLI oder Registry:

1. Neue Klasse, die `aic::IErrorDiffuser` (bzw. `IPaletteQuantizer`)
   implementiert (siehe `renderhamlib/include/aic/dithering/*.hpp` als Vorlage).
2. Am Ende der `.cpp`-Datei: `static AutoRegister<IErrorDiffuser, MeinDiffuser> _reg("mein-name");`
3. Datei in `renderhamlib/CMakeLists.txt` eintragen.

Danach ist der Algorithmus per `--dither mein-name` in der CLI (und via
`Registry<IErrorDiffuser>::instance()` in der GUI) sofort nutzbar.
**Wichtig:** Da die Registrierung über einen statischen Initialisierer
läuft und nie direkt aufgerufen wird, muss jedes Programm, das gegen
`renderhamlib` linkt, `aic_link_whole_archive(<target> renderhamlib)` aus
`cmake/AicWholeArchive.cmake` verwenden (siehe
`tools/renderham/CMakeLists.txt` und `tools/renderhamgui/CMakeLists.txt`) –
sonst wirft der Linker die "ungenutzte" Objekt-Datei einer statischen
Bibliothek weg und der Algorithmus taucht zur Laufzeit nicht in der
Registry auf.

## Bauen (Linux, CMake ≥ 3.16)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Erzeugt `build/tools/renderham/renderHAM` sowie `build/renderhamlib/librenderhamlib.a`.
Wird eine wxWidgets-Installation (Version **3.0 oder neuer**, z. B. Debian/
Ubuntus `libwxgtk3.0-dev`/`libwxgtk3.2-dev` oder ein von CMakes
`find_package(wxWidgets)` erkanntes Äquivalent) gefunden, baut zusätzlich
`build/tools/renderhamgui/renderHAMgui` automatisch mit – fehlt wxWidgets, wird
die GUI sauber übersprungen (CLI/Bibliothek bauen trotzdem). Explizit
abschaltbar mit `-DAIC_BUILD_GUI=OFF`. Für die deutsche Übersetzung wird
zusätzlich `gettext` (`msgfmt`) gesucht – fehlt es, baut die GUI
trotzdem, dann eben nur englischsprachig (siehe GUI-Abschnitt unten).

Das vendorte `third_party/stb` wird als `SYSTEM`-Include-Verzeichnis
eingebunden (`target_include_directories(... SYSTEM ...)`), damit
Compiler-Warnungen aus stbs eigenem Code (z. B.
`-Wmissing-field-initializers` bei dessen `= {0}`-Struct-Nullung) nicht
im Build-Log auftauchen, ohne die vendorte Datei selbst zu verändern
oder diese Warnklasse projektweit abzuschalten. Nutzer meldeten
entsprechende Warnungen auf einer strikteren Toolchain als der
Entwicklungsumgebung; nach der Umstellung liefert ein vollständiger
Neubau von Grund auf `0` Warnungen insgesamt.

## Nutzung

```bash
renderHAM -i foto.jpg -o bild.iff
renderHAM -i foto.jpg -o bild.iff --dither bayer --fit fill --preview vorschau.png
renderHAM --list-dithers
renderHAM --list-quantizers
```

| Option | Bedeutung | Default |
|---|---|---|
| `--crop <geometry>` | Zuschneiden **vor** dem Skalieren, ImageMagick-Stil `WxH+X+Y`. Jeder Wert einzeln als Pixel oder Prozent (`%`) möglich, z. B. `320x200+10+5`, `50%x50%+25%+25%`, gemischt `640x50%+0+10%` | aus |
| `--width/--height` | Zielauflösung | 320×256 (HAM6 Lores PAL) |
| `--fit` | `stretch` \| `fit` (Letterbox) \| `fill` (Crop) | `fit` |
| `--pixel-aspect` | Korrekturfaktor für nicht-quadratische Amiga-Pixel – bewusst nicht hart codiert, siehe `resize.hpp` | `1.0` |
| `--brightness` | Helligkeits-Offset, nur im Ausgabebild sichtbar (additiv, vor Quantisierung) | `0.0` |
| `--contrast` | Kontrast-Faktor um den Mittelpunkt 0,5, nur im Ausgabebild sichtbar | `1.0` |
| `--gamma` | Gamma-Kurve auf den sRGB-kodierten Werten, nur im Ausgabebild sichtbar | `1.0` |
| `--saturation` | Sättigungsfaktor (0 = Graustufen), nur im Ausgabebild sichtbar | `1.0` |
| `--load-palette` | Palette aus Datei laden und sperren (keine Neuquantisierung, keine Verfeinerung) | – |
| `--save-palette` | Tatsächlich verwendete Palette als eigenständige, ladbare ILBM-Datei speichern | – |
| `--fix-color0` | Palette-Index 0 auf einen festen 8-Bit-RGB-Wert erzwingen (`R,G,B`) | – |
| `--dither` | `floyd-steinberg` \| `atkinson` \| `sierra-lite` \| `jarvis-judice-ninke` \| `bayer` \| `blue-noise` \| `none` | `floyd-steinberg` |
| `--quantizer` | `median-cut` \| `kmeans` \| `octree` | `median-cut` |
| `--color-focus` | Pixel-Farbfokus-Karte: `uniform` \| `center` \| `edge` \| `center-edge` \| `golden-ratio` | `uniform` |
| `--palette-iterations` | Iterative Palettenverfeinerung (0 = nur Saat-Palette) | `4` |
| `--chroma-weight` | Farbton- vs. Helligkeitsgewicht in der Encoder-Fehlermetrik (<1 priorisiert Helligkeit) | `1.0` |
| `--no-compress` | ByteRun1-RLE der BODY-Chunk deaktivieren | an |
| `--lace` / `--no-lace` | CAMG-LACE-Bit (Interlace) erzwingen an/aus | Auto: an, wenn `--height` ≥ 400 |
| `--preview` | Zusätzlich dekodiertes PNG schreiben (zeigt exakt, was der Amiga anzeigen würde) | – |

## Wann wirkt `--pixel-aspect` / "Pixel-Seitenverhältnis" wirklich?

Kurz: **nur bei `--fit fit` und `--fit fill`, nie bei `--fit stretch`,
und nie auf die Pixel-Maße der Ausgabedatei** – nur darauf, *wie viel*
vom Quellbild in den Zielrahmen passt bzw. wie stark rundherum
Letterbox/Pillarbox entsteht. Im Detail (`resize.cpp`):

- **Stretch**: `pixel_aspect_ratio` wird gar nicht erst gelesen – der
  Code kehrt bereits davor zurück. Stretch bedeutet "ignoriere jedes
  Seitenverhältnis, fülle exakt die Zielauflösung", da gibt es nichts
  zu korrigieren.
- **Fit/Fill**: Amiga-Anzeigepixel sind (je nach Modus/Overscan/Monitor)
  nicht quadratisch. Der Code behandelt die Zielleinwand für die
  Fit/Fill-Berechnung als `(Zielbreite × Pixel-Seitenverhältnis) ×
  Zielhöhe` statt `Zielbreite × Zielhöhe` – also in einem
  "physischen", quadratpixel-äquivalenten Raum. Das ändert, wie viel
  vom Quellbild sichtbar bleibt (Fill: mehr/weniger wird
  weggeschnitten) bzw. wie breit die Letterbox-Balken werden (Fit) –
  **nicht** die Pixel-Maße der Ausgabe, die bleiben immer exakt
  Zielbreite × Zielhöhe.
- Beispiel: Ziel 320×256 (Pixel-Verhältnis 5:4), Quellfoto im
  Seitenverhältnis 3:2 (1,5:1). Bei `--pixel-aspect 1.0` (Standard,
  "quadratisch angenommen") ist das Quellfoto relativ zum Ziel zu
  breit → Fit-Modus lässt oben/unten Letterbox-Balken. Bei
  `--pixel-aspect 1.2` behandelt die Fit-Berechnung die Zielleinwand
  als `384×256` (= 1,5:1) – passt exakt zum 3:2-Quellfoto, keine
  Letterbox-Balken mehr, obwohl die Ausgabedatei weiterhin 320×256
  Pixel groß ist.
- Betrifft **nicht**: die eigentliche Farbverarbeitung (Dithering,
  Quantisierung, HAM6-Encoding) – der Parameter wirkt ausschließlich
  in der Resize-Stufe, bevor irgendeine Farbentscheidung fällt.
- Es gibt bewusst **keinen** fest hinterlegten "korrekten"
  Amiga-Wert – zu viele monitor-/scandoubler-/overscan-abhängige
  Konventionen, um hier einen falschen Anspruch auf Genauigkeit zu
  erheben (siehe auch die Anmerkung dazu unter "Bekannte Grenzen").
  Wer den GUI-Regler oder `--pixel-aspect` unangetastet lässt (`1.0`),
  bekommt exakt das bisherige, unkorrigierte Verhalten.
- **GUI**: der Regler ist bei "Stretch" als Skalierungsmodus
  automatisch deaktiviert (ausgegraut, `ParameterPanel::UpdatePixelAspectEnabled`,
  ausgelöst bei jedem Moduswechsel und einmal nach `LoadDefaults`) –
  genau deshalb, weil er dort ohnehin nichts bewirken würde. Per
  Debug-Log bestätigt: `enabled=1` bei Fit (Standard), `enabled=0`
  nach Wechsel zu Stretch, wieder `enabled=1` nach Wechsel zurück zu
  Fill.

## Testen / Validieren

`testdata/make_test_image.py` erzeugt ein synthetisches Testbild
(Regenbogenverlauf, Kugel-Shading, gesättigte Farbflächen, feines
Schachbrett, Graustufenverlauf als HAM-Fringing-Stresstest).

`testdata/verify_ilbm.py` parst eine erzeugte `.iff` unabhängig vom
C++-Code (eigener ByteRun1-Decoder) und prüft Chunk-Größen, BMHD-Felder,
CAMG-HAM-Flag und dass komprimierte/unkomprimierte Variante byteidentische
Planar-Daten ergeben.

`testdata/decode_ham6_independent.py` ist die schärfste Probe: eine von
Grund auf neu geschriebene Python-Implementierung von Planar→Chunky→
HAM6-Decode, verglichen gegen die `--preview`-PNG des C++-Tools. Bei der
Referenzkonvertierung: 100 % pixelidentisch.

```bash
python3 testdata/make_test_image.py
./build/tools/renderham/renderHAM -i testdata/test_input.png -o /tmp/test.iff --preview /tmp/preview.png
python3 testdata/verify_ilbm.py /tmp/test.iff /tmp/test.iff
python3 testdata/decode_ham6_independent.py /tmp/test.iff /tmp/preview.png /tmp/independent.png
```

## GUI (wxWidgets)

Läuft mit wxWidgets **3.0 bis 3.2** (entwickelt/getestet mit 3.2.4;
zusätzlich der komplette GUI-Quellcode per `-fsyntax-only` gegen echte,
selbst aus dem Quellcode konfigurierte wxWidgets-3.0.5-Header geprüft –
keine Anpassung nötig, da alle verwendeten Klassen (`wxSpinCtrlDouble`,
`wxCmdLineParser::AddParam`, ...) bereits seit 3.0 unverändert existieren.
CMakes `find_package(wxWidgets)` sucht ohnehin automatisch auch nach
versionierten Binärnamen wie `wx-config-3.0`).

`tools/renderhamgui/` ist ein plattformunabhängiges wxWidgets-Frontend, das
dieselbe `aic::ConversionPipeline` nutzt wie die CLI – kein separater
Konvertierungscode.

**Architektur:**
- `ParameterPanel` – alle Regler (Zuschnitt, Zielauflösung/Modus,
  Dithering/Palette/Wichtigkeit, Verfeinerung, Farbgewicht, Ausgabe) als
  eigenständiges, wiederverwendbares Panel. Dropdowns werden zur
  Laufzeit aus der `Registry<T>` befüllt (`--list-dithers` &c. lassen
  grüßen) – ein neuer Ditherer taucht automatisch auf, ohne GUI-Code
  anzufassen. Bewusst textarm: Zuschnitt über vier Spinner +
  Pixel/Prozent-Umschalter statt der rohen `WxH+X+Y`-Zeichenkette,
  Zielauflösung über Presets, Fit-Modus/LACE über Radio-Gruppen.
- `ImageView` – eigenständiges `wxScrolledWindow`-Widget für Quell- und
  Ergebnis-Vorschau: passt sich automatisch an die Panelgröße an
  (`fit`-Modus, aktiv bis der Nutzer manuell zoomt), rendert immer per
  Nearest-Neighbor (harte HAM6-Pixelkanten werden nie geglättet
  vorgetäuscht). Zoom ist pro Instanz optional aktivierbar
  (`SetZoomEnabled`) – bei der Quelle aus, beim Ergebnis an, inkl.
  Strg+Mausrad, Buttons und Tastenkürzeln (Strg +/-, Strg+0 = 100 %,
  Strg+9 = einpassen) bis Pixelebene.
- `MainFrame` – Datei öffnen/speichern, Quell-/Amiga-Vorschau-Vergleich
  nebeneinander, Statuszeile, `F5` als Tastenkürzel fürs Konvertieren.
  Akzeptiert optional einen Bildpfad als Kommandozeilenargument
  (`renderHAMgui foto.png`), um direkt mit geladenem Bild zu starten.
- `aic::ConversionPipeline::preview()` – neue, für die GUI eingeführte
  Methode: läuft Laden→Crop→Skalieren→HAM6-Encoding komplett im
  Speicher, **ohne** etwas auf die Platte zu schreiben. `convert()` ist
  jetzt ein dünner Wrapper darüber (ruft `preview()` und schreibt
  danach die Datei) – die CLI verhält sich dadurch unverändert
  (bytegleich getestet), die GUI kann aber bei jeder Reglerbewegung neu
  rendern, ohne bei jedem Klick eine Datei zu erzeugen.

**Behobene GTK-Warnung beim Start** (`gtk_box_gadget_distribute:
assertion 'size >= 0' failed in GtkSpinButton`): mehrere Zahlenfelder
und die Zoom-Buttons hatten eine explizit zu kleine feste Breite
(`wxSize(85,-1)` bzw. `wxSize(28,-1)`) – kleiner als das native
GTK-Mindestmaß für diese Widgets bei manchen Themes/Schriftgrößen, was
die interne Box-Layout-Berechnung negativ werden ließ. Fix: feste
Breiten entfernt, Widgets nutzen ihre natürliche Mindestgröße
(`wxDefaultSize`). Verifiziert durch direkten Vorher/Nachher-Vergleich
der Log-Ausgabe unter Xvfb – Warnung komplett verschwunden.


**Hintergrund-Thread (seit A4, siehe Abschnitt „Konvertierung im Hintergrund“):** `RunConversion()`
übergibt `ParameterPanel::GetOptions()` + `pipeline_.preview()` – die vollständige, in sich
geschlossene Arbeitseinheit – an einen Arbeits-Thread und bekommt das Ergebnis per `CallAfter` zurück.
Noch offen ist nur die entprellte Live-Vorschau bei jeder Parameteränderung.

**Vorbereitet für spätere Ausbaustufen** (beide noch nicht umgesetzt):
- *Visueller Zuschnitt*: `ImageView` bringt durch die Zoom/Scroll-Infrastruktur
  schon die Grundlage mit (Bildkoordinaten ↔ Bildschirmkoordinaten sind
  bereits ineinander umrechenbar); ein Rubber-Band-Rechteck auf der
  Quellvorschau müsste nur dieselben `CropSpec`-Werte lesen/schreiben
  wie die vier Spinner in `ParameterPanel` – zusätzlicher Eingabeweg,
  keine Restrukturierung.
- *Batch-Betrieb*: `ParameterPanel` ist bereits nicht an "genau eine
  Datei" gekoppelt – eine Dateiliste könnte dieselben Regler als
  Vorlage für mehrere Dateien nutzen, mit `ConversionOptions → convert()`
  pro Datei in einer Schleife oder einem Worker-Pool.

**Verifiziert** (Xvfb + fluxbox, `xdotool` zur Bedienungs-Simulation,
`import` für Screenshots, alles ohne echtes Display): App baut und
startet real, Bild laden funktioniert (auch per Kommandozeilenpfad),
`F5` löst tatsächlich eine Konvertierung aus (Vorschau-Bereich wechselt
messbar von einfarbigem Platzhalter zu echtem HAM6-Dithering-Inhalt,
Standardabweichung 4,3 → 98,2), "Speichern unter" über den nativen
Datei-Dialog (per `xdotool` samt Tastatur-Navigation bedient) schreibt
eine echte `.iff`-Datei. Diese Datei ist **bytegleich** mit der
CLI-Ausgabe für dieselben Parameter und besteht dieselbe
Verifikationskette wie die CLI-Tests (eigener Struktur-Validator,
echter externer Decoder `netpbm`s `ilbmtoppm`). Zusätzlich: Fenster
per `xdotool windowsize` vergrößert – Bildinhalt wächst nachweislich
mit (farbige Bildfläche wuchs proportional zur Fenstergröße). Zoom per
Tastenkürzel getestet (Debug-Build mit Log-Ausgabe der tatsächlichen
Zoomstufe): fünfmal Strg+Plus ergab 105%→158%→237%→356%→534% (exakt
Faktor 1,5 pro Schritt), zweimal Strg+Minus wieder exakt zurück auf
237%.

### Kritische Durchsicht des Bedienkonzepts: Menü entfernt, Save-Überladung und Namenskollision behoben

Auf Anfrage eine ehrliche Durchsicht des bestehenden Layouts/Bedienkonzepts
für die beiden Hauptanwendungsfälle (Einzelbild-Konvertierung,
Animationserzeugung) durchgeführt. Drei konkrete Punkte identifiziert
und behoben:

**1. Menü entfernt, Settings-Button eingeführt.** Es gab eine
Menüleiste ("File": Open, Save As, Settings, Exit) neben einem
Toolbar mit denselben Aktionen als Buttons – Open und Save As waren
damit doppelt vorhanden, Exit redundant zum normalen
Fenster-Schließen, und nur "Settings" (Sprachumschaltung) war
**ausschließlich** über das Menü erreichbar, ohne sichtbaren Button.
Das war weder konsequent knopfbasiert noch konsequent menübasiert,
sondern ein unentschiedenes Dazwischen. Menüleiste komplett entfernt,
stattdessen ein "Settings..."-Button im Toolbar ganz rechts.

Beim Umbau einen selbst verursachten Fehler gefunden und korrigiert:
die erste Fassung wandelte die bisherigen `Bind(wxEVT_MENU, ...)`-Aufrufe
für Open/Save-As einfach in `Bind(wxEVT_BUTTON, ...)` um – das hätte zu
doppelt feuernden Handlern geführt, da diese Buttons bereits eigene,
direkte Bindungen an sich selbst hatten (die `wxEVT_MENU`-Bindungen
waren ausschließlich für die jetzt entfernten Menüeinträge da). Bereinigt:
jeder Button hat jetzt genau eine Bindung, konsistent für alle sechs
Toolbar-Buttons.

**2. Save-Button-Überladung behoben.** Bei aktivierter Animation
änderten "Save"/"Save As..." bereits bisher stillschweigend ihre
Bedeutung (kompletter Mehrbild-Export statt Einzelbild-Speichern),
ohne dass die Beschriftung das widerspiegelte – eine echte Falle für
versehentliche Fehlbedienung. Beide Buttons beschriften sich jetzt
dynamisch um: "Export Animation"/"Export Animation As..." bei
aktivierter Animation, sonst wie gehabt "Save"/"Save As...". Neuer
Callback-Mechanismus in `ParameterPanel`
(`SetAnimationToggledCallback`, nach demselben Muster wie der
bestehende Crop-Changed-Callback) benachrichtigt `MainFrame` bei jedem
Umschalten von "Enable Animation". Zusätzlich ein Tooltip auf "Enable
Animation", der die bisher unerklärte Abhängigkeit von "Enable crop"
benennt.

**3. Namenskollision "Output"/"Output" aufgelöst.** Es gab zwei
Regel-Gruppen namens "Output" im Layout – einmal in der
Animation-Box (Single Images/Anim-File), einmal als eigene Box rechts
unten (ByteRun1-Kompression/Interlace). Die Animation-interne Gruppe
heißt jetzt "Export format".

**4. Lock Palette überschreibt jetzt sichtbar statt stillschweigend.**
Bei aktiver Palettensperre werden Quantisierung *und* Verfeinerung
komplett übersprungen (siehe `HAM6Encoder::encode`) – "Palette
quantizer", "Color focus", "Refinement" und "Chroma↔Luminance" in
"HAM conversion settings" hatten dadurch schlicht keine Wirkung mehr,
blieben aber normal bedienbar, ohne das erkennbar zu machen.
`UpdatePaletteControlsEnabled()` graut diese vier Regler jetzt aus,
sobald "Lock Palette" aktiv ist (Dithering bleibt unverändert bedienbar,
da es unabhängig von gesperrter/frischer Palette weiterhin wirkt).

**Verifiziert:**
- Menüleiste per Screenshot bestätigt entfernt, Settings-Button
  bestätigt funktionsfähig (öffnet den Sprachdialog)
- Dynamische Save-Umbenennung per Screenshot bestätigt: "Export
  Animation"/"Export Animation As..." erscheinen korrekt nach
  Aktivieren von "Enable Animation"
- "Export format" korrekt umbenannt, per Screenshot bestätigt
- Für Lock Palette galt dieselbe wiederkehrende
  Klick-Zuverlässigkeits-Einschränkung dieser Sandbox-Umgebung wie
  zuvor beim Remove-Keyframe- und Anim-File-Radiobutton-Fall – über
  einen temporären Debug-Hotkey in beide Richtungen exakt bewiesen:
  Sperre aktiviert → alle vier Regler deaktiviert; Sperre wieder
  deaktiviert → alle vier Regler wieder aktiviert
- Präprozessor-Direktiven nach Debug-Code-Entfernung auf Balance
  geprüft (0 ifdef / 0 endif je Datei)
- CLI-Baseline unverändert (`0,0126422`), Release-Build sauber (0
  Warnungen), breiter GUI-Sanity-Lauf ohne neue Warnungen

**Erneute Kompaktheitsanalyse nach diesen Änderungen** (wie angefragt):
die drei Fixes ändern kaum etwas an der reinen Platzbilanz – die
Menü-Entfernung gibt minimal vertikalen Raum zurück, die
Lock-Palette-Änderung ist rein visuell (ausgegraut statt entfernt,
keine Abmessungsänderung). Ein neuer Kompromiss kommt durch die
Save-Umbenennung hinzu: "Export Animation As..." ist spürbar länger
als "Save As...", wodurch der Toolbar bei aktiver Animation breiter
wird (Fenster verbreitert sich dynamisch mit) – hier wurde bewusst
Klarheit über Kompaktheit gestellt. Die bereits zuvor identifizierten
Kompaktheits-Gelegenheiten bleiben unverändert bestehen, da diese
Änderungsrunde sie nicht angefasst hat: Label-über-Feld statt
Label-neben-Feld bei Width/Height/X/Y und den drei
HAM-conversion-settings-Dropdowns; "Single Images"/"Anim-File" steht
weiterhin vertikal statt horizontal wie das strukturell gleiche
"Pixels"/"Percent"; "Scaling mode"/"Interlace (LACE)" stehen ebenfalls
weiterhin vertikal. Die räumliche Trennung von
Palettenberechnungs-Reglern (HAM conversion settings) und
Palettensperre (Palette-Box) bleibt bestehen – das ist eine
Layout-*Organisations*-Frage, keine Kompaktheits-Frage, und war nicht
Teil dieser Änderungsrunde.

### Vier weitere GUI-Verbesserungen

- **Gleiche Vorschaugrößen**: Quelle und Amiga-Vorschau hatten
  unterschiedliche Höhen, weil nur die Ergebnis-Spalte eine
  Zoom-Button-Kopfzeile hatte. Fix: `MainFrame::BuildPreviewHeader()`
  baut für beide Spalten dieselbe Kopfzeile (Zoom jetzt auch für die
  Quelle aktiviert). Nebenbei einen 4px-Breitenunterschied durch einen
  einseitigen Sizer-Rand gefunden und durch einen echten `AddSpacer()`
  ersetzt. Verifiziert per direktem Größen-Log: `560x180 = 560x180`,
  auch nach Resize identisch.
- **Drag-to-Pan im Zoom**: `ImageView` unterstützt jetzt Maus-Drag zum
  Verschieben des sichtbaren Ausschnitts (nur wenn Zoom aktiviert ist),
  unabhängig vom optionalen Zuschnitt-Rubberband. Per Debug-Log
  verifiziert: Scroll-Position ändert sich korrekt während des Drags.
- **Zuschnitt-Rubberband, bidirektional editierbar**: Ist "Zuschnitt
  aktivieren" angehakt, zeigt die Quellvorschau ein gelbes Rechteck mit
  8 Anfass-Punkten (4 Ecken, 4 Kantenmitten) – ziehen bewegt/verändert
  es. Läuft über zwei Callbacks: `ParameterPanel::SetCropChangedCallback`
  (Spinner/Checkbox → Rechteck) und `ImageView::SetCropEditedCallback`
  (Rechteck-Drag → Spinner), mit einem Re-Entrancy-Guard gegen
  Rückkopplungsschleifen. Nebenfix dabei: Umschalten zwischen Pixel und
  Prozent behält jetzt das tatsächliche Rechteck bei (rechnet um), statt
  dieselben Zahlen einfach neu zu interpretieren. Verifiziert per
  Debug-Log: Checkbox-Klick erzeugt Overlay `(0,0) 100x100`; Ziehen am
  Eck-Handle erzeugt `overlay->params: (0,0) 389x320`, und ein
  anschließendes `GetOptions()` bestätigt `w=389.0px h=320.0px` – die
  Spinner haben exakt den gezogenen Wert übernommen.
- **Drag-and-Drop zum Laden**: `wxFileDropTarget` auf Hauptfenster und
  beiden Vorschau-Panels registriert (Standardmuster, `wx/dnd.h`).
  **Nicht end-to-end testbar** in dieser Sandbox – das X11-XDND-Protokoll
  lässt sich mangels echter Drag-Quelle nicht über `xdotool` simulieren.
  Der Code folgt der dokumentierten wxWidgets-API exakt, ist aber nur
  bis Kompilierebene geprüft, nicht interaktiv.

### Zuschnitt außerhalb der Bildgrenzen

Vorbereitend für die Animations-Keyframes: der Zuschnittsbereich lässt
sich jetzt auch teilweise oder vollständig außerhalb des Quellbilds
positionieren – über jeden Rand hinausragend, oder sogar komplett
daneben. Das ermöglicht später bei einer Keyframe-Animation ein
"Hineinwandern" in das Bild (Startpunkt außerhalb, Endpunkt innerhalb)
oder "Herauswandern" (umgekehrt). Betrifft sowohl die
Zahlenfelder (X/Y erlauben jetzt negative Werte) als auch das
Rubber-Band-Ziehen im Quellbild (`ClampRectToImage` sowie die
ratio-gesperrte Größenänderung klemmen nicht mehr auf die
Bildgrenzen – nur noch eine Mindestgröße bleibt erzwungen).

Jeder Pixel des Zuschnittsbereichs ohne entsprechenden Quell-Pixel
wird mit einer Hintergrundfarbe gefüllt. Diese Farbe folgt immer dem
"Fix color 0"-Farbwähler im Palette-Bereich – unabhängig davon, ob die
zugehörige Checkbox angehakt ist (die Checkbox steuert nur, ob die
Farbe zusätzlich in die Ausgabe-Palette erzwungen wird). Per Default
zeigt dieser Farbwähler Schwarz, die Füllung ist also standardmäßig
schwarz. Konzeptionell passend: auf echter Amiga-Hardware ist
Farbregister 0 ohnehin zugleich die Rand-/Hintergrundfarbe außerhalb
der eigentlichen Bitmap-Fläche.

`crop_image()` in der Bibliothek klemmt den Zuschnittsbereich dafür
nicht mehr auf die Quellbild-Grenzen; stattdessen bekommt jeder
Ausgabe-Pixel entweder den entsprechenden Quell-Pixel oder die
konfigurierte Füllfarbe. Ein komplett außerhalb liegender
Zuschnittsbereich ist jetzt kein Fehler mehr, sondern liefert ein
vollständig mit der Füllfarbe gefülltes Bild.

**Verifiziert**:
- Eigenständiges Testprogramm: ein Zuschnittsbereich, der zur Hälfte
  links/oben aus einem 100×100-Testbild herausragt, liefert exakt die
  angeforderte Zielgröße; der herausragende Teil zeigt exakt die
  Füllfarbe, der übrige Teil exakt den echten Quellbild-Inhalt. Ein
  komplett außerhalb liegender Bereich liefert ein durchgehend mit der
  Füllfarbe gefülltes Bild, keinen Fehler.
- GUI-Bereich für X/Y per Debug-Log bestätigt: bei einem 640 Pixel
  breiten Quellbild jetzt `-640` bis `640` (vorher `0` bis `640`).
- Echte Konvertierung mit X=-30 (bei 100 Pixel Zuschnittsbreite, also
  30 % außerhalb) durchgeführt und das Ergebnis visuell geprüft: der
  linke Bereich ist durchgehend die Füllfarbe, der rechte Bereich
  zeigt den echten Bildverlauf – exakt an der erwarteten Stelle.
- Derselbe Test mit einer über die Konfigurationsdatei gesetzten,
  eindeutig unterscheidbaren Füllfarbe (Grün) bei **deaktivierter**
  "Fix color 0"-Checkbox wiederholt: die Füllung erscheint trotzdem
  grün – bestätigt, dass die Füllfarbe unabhängig vom Checkbox-Status
  immer dem Farbwähler folgt.
- CLI-Baseline unverändert (`0,0126422`), Release-Build sauber, breiter
  GUI-Sanity-Lauf ohne neue Warnungen.

**Nachtrag – zwei zusammenhängende Fehler behoben, plus automatisches
erstes Keyframe:**

1. **Width/Height-Zahlenfelder blieben auf die Quellbild-Größe
   begrenzt**, obwohl das Rubber-Band-Ziehen (siehe oben) bereits
   größere Bereiche erlaubte – eine echte Inkonsistenz zwischen beiden
   Bedienwegen. `UpdateCropSpinRanges()` erlaubte zwar negative X/Y-Werte,
   ließ den Maximalwert für Breite/Höhe aber unverändert bei der
   Quellbild-Größe. Behoben: alle vier Felder (Width/Height/X/Y)
   erlauben jetzt großzügig das Vierfache der jeweiligen
   Quellbild-Dimension.

2. **Keyframes verloren ihren Zuschnittsbereich beim Wechseln** – eine
   direkte Folge von Fehler 1, keine eigenständige Ursache:
   `SetCropPixelRect()` ruft beim Laden eines Keyframes
   `crop_w_->SetValue(...)` auf; lag der gespeicherte Wert über dem
   (zu engen) Grenzwert des Zahlenfelds, klemmte `wxSpinCtrlDouble`
   ihn beim Anzeigen still auf das Maximum – und da eine Auswahl auch
   bedeutet, dass Änderungen zurückgeschrieben werden (siehe oben),
   überschrieb `NotifyCropChanged()` direkt im Anschluss den
   *korrekten* gespeicherten Wert mit dem *geklemmten*. Das bloße
   Anschauen eines Keyframes mit zu großem Wert genügte also, um ihn
   dauerhaft zu beschädigen. Mit der Korrektur von Fehler 1 behoben,
   da beide dieselbe Wurzel hatten.

3. **Automatisches "Keyframe 1"**: Wird "Enable Animation" aktiviert
   und es existiert noch kein Keyframe, wird jetzt automatisch eines
   aus dem aktuell eingestellten Zuschnittsbereich angelegt (nicht
   mehr zusätzlich ausgewählt, siehe Nachtrag weiter unten zum
   Auto-Auswahl-Fehler) – die Erfassungslogik wurde dafür aus
   `OnAddKeyframe` in eine gemeinsam genutzte Methode
   `AddKeyframeFromCurrentCrop()` ausgelagert. Bisher startete eine
   neue Animation mit einer leeren Liste, was einen zusätzlichen
   "jetzt erst mal hinzufügen"-Schritt erforderte.

**Verifiziert**:
- Zahlenfeld-Bereich per Debug-Log bestätigt: bei 640×400 Quellbild
  jetzt `w_max=2560` (`h_max=1600`), `x_min=-2560`/`x_max=2560` –
  korrekt das Vierfache der jeweiligen Dimension
- Automatisches Keyframe 1 per Debug-Log bestätigt: vor Aktivierung
  von Animation `count=0`, unmittelbar danach `count=1` mit dem
  aktuell eingestellten Zuschnittsbereich
- **Wichtigster Test für Fehler 2**: Keyframe 1 auf Breite 700 gesetzt
  (über die Quellbreite 640 hinaus), Keyframe 2 mit Breite 50
  hinzugefügt, zurück zu Keyframe 1 gewechselt – Breite weiterhin
  exakt `700`, weder beim Anzeigen geklemmt noch beim Zurückschreiben
  beschädigt
- CLI-Baseline unverändert (`0,0126422`), Release-Build sauber (0
  Warnungen), breiter GUI-Sanity-Lauf ohne neue Warnungen

### Zuschnitt: Seitenverhältnis sperren

Zusätzlich zum freien Rubber-Band-Zuschnitt: eine Checkbox "Lock aspect
ratio" plus ein Dropdown mit fünf Presets: `NTSC` 8:5 und `PAL` 5:4 –
dieselben Pixel-Verhältnisse wie die gleichnamigen
Zielauflösungs-Presets (320:200 bzw. 320:256) – sowie die
Standard-Formate `1:1`, `4:3` und `16:9`. Wirkt sowohl auf die
Zahlenfelder als auch auf das visuelle Ziehen:

- **Zahlenfelder**: Ändern von Breite oder Höhe rechnet die jeweils
  andere Dimension automatisch nach (`ParameterPanel::ApplyRatioFromWidth`/
  `ApplyRatioFromHeight`) – konsequent über Pixel-Raum, unabhängig davon,
  ob gerade Pixel oder Prozent als Einheit gewählt ist (Breite-% und
  Höhe-% beziehen sich auf unterschiedliche Quell-Dimensionen, ein
  direktes Sperren der Prozentwerte wäre falsch).
- **Rubber-Band-Ziehen**: `ImageView::ResizeWithLockedRatio` fixiert die
  dem gezogenen Anfasspunkt gegenüberliegende Ecke als Anker, wählt je
  nach Anfasspunkt die "treibende" Achse (Breite bei Ost/West-Kanten,
  Höhe bei Nord/Süd-Kanten, bei Ecken die Achse, die die größere Box
  ergibt) und skaliert am Bildrand **beide** Dimensionen gemeinsam
  herunter statt sie unabhängig zu kappen – ein unabhängiges Kappen
  würde das Verhältnis verfälschen.
- Aktivieren der Sperre oder Wechsel des Presets passt das aktuell
  gewählte Rechteck sofort an (Breite bleibt, Höhe wird neu berechnet).

**Verifiziert** per Debug-Log (Xvfb + fluxbox + `xdotool`):
- Sperre aktivieren snapt `100×100` sofort korrekt auf `100×62`
  (100/1.6 = 62.5 → 62) bei NTSC-Verhältnis.
- Breite→Höhe über den Spin-Pfeil: Breite 100→101 aktualisiert Höhe
  korrekt auf 63 (101/1.6 = 63.1).
- Rubber-Band-Ziehen an der SO-Ecke: Zwischenschritte `224×140`
  (Verhältnis exakt 1.6000) und `380×238` (1.5966, übliche
  Pixel-Rundung) – Verhältnis bleibt während des gesamten Ziehens
  erhalten.
- Rubber-Band-Ziehen an der Ost-Kante über den Bildrand hinaus:
  Ergebnis klemmt exakt auf `640×400` – die volle Bildgröße, die
  zufällig selbst ein perfektes 1.6-Verhältnis hat, was bestätigt,
  dass beide Dimensionen gemeinsam (nicht unabhängig) an die Grenze
  herangeführt wurden.
- Höhe→Breite über den Spin-Pfeil war per `xdotool`-Klick nicht
  eindeutig reproduzierbar zu testen (mehrere Versuche ergaben
  uneinheitliche Klick-Treffer, vermutlich GTK-Spin-Button-Wiederholungs-
  verhalten unter Xvfb) – **nicht separat empirisch bestätigt**, aber
  `ApplyRatioFromHeight` ist strukturell identisch zu `ApplyRatioFromWidth`
  (nur Breite/Höhe vertauscht) und nutzt denselben, für die
  Breite-zu-Höhe-Richtung bereits bewiesenen Mechanismus
  (`event.GetId()`-Vergleich in `OnCropSpinChanged`).

**Verworfene Zwischenlösung – Interlace-Presets**: eine frühere Version
hatte hier zusätzlich `NTSC Interlaced`/`PAL Interlaced`-Presets, die
das Verhältnis so berechneten, dass ein Zuschnitt nach dem Skalieren
auf eine Interlace-Zielauflösung physisch korrekt proportioniert
erscheint (Amiga-Interlace hat zwar doppelte Pixelzahl in der Höhe,
belegt auf echter Hardware aber dieselbe physische Bildhöhe wie
Nicht-Interlace). Nach genauerer Überlegung ergab dieses Konzept für
ein reines Zuschnitt-Werkzeug keinen Sinn (vermischt eine
Display-physikalische Korrektur mit einer sonst rein
quellpixel-basierten Auswahl) und wurde wieder entfernt, zugunsten der
Standard-Formate 1:1/4:3/16:9.

### Overscan-Presets für die Zielauflösung

Die Zielauflösungs-Dropdown-Liste (GUI, "Target resolution and mode")
enthält zu jedem der vier bestehenden Presets jetzt zusätzlich eine
Overscan-Variante: `352×280` (PAL), `352×240` (NTSC), `352×560` (PAL,
interlaced), `352×480` (NTSC, interlaced). Wie beim
Pixel-Seitenverhältnis gibt es dafür **keinen** einzigen
hardware-vorgegebenen Wert – die tatsächlich sichtbare
Overscan-Fläche hängt vom konkreten Monitor/Scandoubler ab; `352`
Breite ist eine verbreitet zitierte "sichere Overscan"-Zahl für PAL
wie NTSC, `280`/`240` (und ihre verdoppelten Interlace-Gegenstücke
`560`/`480`) sind gängig zitierte Vollbild-Höhen. Reine
Komfort-Presets für Breite/Höhe – wer eigene Werte braucht, nutzt
weiterhin "Custom...".

**Verifiziert** per Debug-Log: erstes Overscan-Preset (Index 1)
ausgewählt → `target=352×280`; letztes (Index 7) ausgewählt →
`target=352×480` – beide Enden der neuen Liste bestätigt korrekt.

### Realistischere Interlace-Vorschau

Ein Amiga-Interlace-Modus (z. B. PAL Interlace, 320×512) hat zwar die
doppelte Pixelzahl in der Höhe, belegt auf einem echten Röhrenbildschirm
aber dieselbe physische Bildhöhe wie der nicht-interlaced Modus – die
beiden Halbbilder werden abwechselnd angezeigt, nicht übereinander
gestapelt. Eine einzelne Interlace-Scanline ist also nur halb so
**hoch** wie eine nicht-interlaced Scanline – nicht etwa jede zweite
Zeile schlicht nicht vorhanden.

Checkbox "Simulate interlace (half height)" direkt über der
Amiga-Vorschau (nur dort – für die Quelle ergibt das Konzept keinen
Sinn, daher kein entsprechendes Kästchen auf der linken Seite, aber ein
exakt vermessener unsichtbarer Platzhalter derselben Höhe, damit beide
Vorschau-Panels weiterhin gleich groß bleiben, siehe "Gleiche
Vorschaugrößen" oben). Angehakt, ruft `MainFrame::UpdatePreviewDisplay`
`ImageView::SetVerticalSquash(0.5)` auf – eine rein **darstellungs**bezogene,
anisotrope Stauchung der Y-Achse (Breite bleibt unangetastet). Die
zugrundeliegenden Bilddaten (`image_` in `ImageView`) werden dabei
**nicht verändert**: alle Zeilen des dekodierten Bilds bleiben
vollständig erhalten und beim Hineinzoomen inspizierbar, nur die
Render-Höhe jeder Zeile wird halbiert.

**Nachtrag 1 – erste Implementierung zeigte quadratische statt halbhohe
Pixel und verwarf Bilddaten**: die ursprüngliche Version skalierte das
Bild NICHT bei der Anzeige, sondern verkleinerte es vorab durch
direktes Kopieren jeder zweiten Zeile (`memcpy`, um die zu dem Zeitpunkt
bereits bekannte `wxImage::Scale()`-Weichzeichnung zu vermeiden – siehe
Nachtrag 2). Das hatte zwei Probleme, die ein Nutzer beim Hineinzoomen
bemerkte: die **Hälfte der tatsächlich dekodierten Bilddaten ging
verloren** (jede zweite Zeile wurde schlicht nicht mehr angezeigt), UND
da `ImageView` beim Zoomen gleichmäßig in beide Richtungen skaliert,
erschienen die verbliebenen Pixel beim Hineinzoomen **quadratisch**
statt – wie erwartet – halb so hoch wie breit. Der jetzige Ansatz
(Stauchung der Darstellung statt Verwerfen von Zeilen) behebt beides
auf einmal: keine Daten gehen verloren, und die Pixelform stimmt bei
jeder Zoomstufe.

**Nachtrag 2 – `wxImage::Scale()` erwies sich unabhängig vom
Qualitätsparameter als unzuverlässig**: beim Umbau auf die
Stauchungs-Darstellung fiel auf, dass `ImageView::RebuildScaledBitmap`
für die eigentliche Zoom-Bitmap ebenfalls `wxImage::Scale(...,
wxIMAGE_QUALITY_NEAREST)` nutzte – exakt die Funktion, die schon für
Nachtrag 1 als weichzeichnend beim Verkleinern um Faktor 2 entlarvt
worden war (ein Testbild mit abwechselnd reinen rot/blauen Zeilen ergab
nach der Skalierung durchgehend Lila-Mischtöne). Bei aktiver Stauchung
UND passendem Gesamt-Zoomfaktor hätte genau dieser Fall erneut
auftreten können – die Vorschau wäre wieder verschwommen gewesen, trotz
des neuen, datenerhaltenden Ansatzes. Direkt reproduziert: derselbe
Rot/Blau-Test gegen den *tatsächlichen* Produktionscode ergab erneut
`(127,0,127)` statt reinem Rot/Blau. Behoben durch eine **manuelle,
garantiert exakte Nearest-Neighbor-Abtastung** (jedes Ausgabepixel
kopiert die exakten Bytes genau eines Quellpixels, kein
`wxImage::Scale()` mehr an dieser Stelle überhaupt) – das behebt
nebenbei einen latenten Fehler im ganz normalen Zoomen (jenseits von
Interlace), der schon vorher bestand, aber bisher nicht auffiel.

Wirkt nur, wenn das aktuelle Ergebnis tatsächlich interlaced ist
(`PreviewResult::lace`) – angehakt bei einem nicht-interlaced Ergebnis
ist es ein harmloses No-op, kein Sonderfall mit eigenem
Enabled/Disabled-Zustand nötig. Umschalten der Checkbox rendert das
bereits vorhandene Ergebnis sofort neu, ohne die Konvertierung erneut
laufen zu lassen.

**Verifiziert**:
- Isolierter Test mit dem exakten neuen Skalierungscode (Rot/Blau-Testbild,
  derselbe Fall, der `wxImage::Scale()` zuvor zum Mischen brachte):
  durchgehend reines Rot, kein Mischton – Fehler behoben.
- Per Debug-Log am echten dekodierten `320×512`-Ergebnis: die
  zugrundeliegenden Bilddaten bleiben in JEDEM Fall `320×512`
  (unangehakt wie angehakt) – keine Zeile geht verloren.
- Im echten manuellen 100%-Zoom (nicht Fit-Modus, direkt per
  `Ctrl+0` bestätigt): ohne Stauchung `virtual_size=443×512` (Höhe
  exakt 512, 1:1), mit Stauchung `virtual_size=443×256` – Breite
  bleibt exakt gleich (443), Höhe wird exakt halbiert (512→256). Jedes
  einzelne Pixel ist damit bei jeder Zoomstufe tatsächlich halb so
  hoch wie breit, nicht quadratisch.
- Fit-Modus-Zoom passt sich beim Umschalten der Stauchung konsistent
  an (verdoppelt sich sinngemäß, da die effektive Höhe für die
  Fit-Berechnung nun halbiert in die Rechnung eingeht).

### Mehrsprachigkeit (i18n)

Alle sichtbaren GUI-Strings sind auf **Englisch als Quellsprache**
umgestellt (Standard beim Start, unabhängig von der System-Locale) und
mit dem wxWidgets-Standardmakro `_()` markiert. Deutsch ist als erste
zusätzliche Sprache vollständig vorhanden; die Architektur ist bewusst
so gebaut, dass weitere Sprachen später nur eine neue `.po`-Datei
brauchen, keine Code-Änderung:

- `tools/renderhamgui/po/renderhamgui.pot` – mit `xgettext --keyword=_` extrahiertes
  Vorlage. Bei neuen `_()`-Strings im Code einfach neu extrahieren
  (`xgettext ... --output=po/renderhamgui.pot *.cpp`) und per
  `msgmerge --update po/de.po po/renderhamgui.pot` in die bestehenden
  Übersetzungen einpflegen (erhält vorhandene Übersetzungen, markiert
  geänderte/neue Strings).
- `tools/renderhamgui/po/de.po` – deutsche Übersetzung, alle 60 Strings.
- CMake kompiliert `.po`→`.mo` automatisch beim Bauen
  (`msgfmt`, `AIC_GUI_LOCALES`-Liste in `tools/renderhamgui/CMakeLists.txt` –
  eine neue Sprache dort einfach ergänzen) nach
  `translations/<sprache>/LC_MESSAGES/renderhamgui.mo`, relativ zur
  Programmdatei gesucht (`Localization.cpp`). Fehlt `msgfmt` auf dem
  Build-Rechner, baut die GUI trotzdem – nur eben englischsprachig.
- **Settings-Dialog** (Datei-Menü → "Se&ttings..." / "Ein&stellungen...")
  zeigt eine dynamisch ermittelte Sprachliste (jede vorhandene
  `translations/<code>/LC_MESSAGES/renderhamgui.mo`, plus Englisch immer an
  erster Stelle). wxWidgets übersetzt bereits erzeugte Widgets nicht
  automatisch neu, wenn sich die aktive `wxLocale` ändert – ein Neustart
  ist die robustere Lösung als jedes Label manuell zu aktualisieren.
  Die Anwendung startet sich dafür selbst neu (`wxExecute` mit
  `--lang <code>` und dem aktuell geöffneten Bildpfad, falls vorhanden),
  die Auswahl wird über `wxConfig` gespeichert (unter Linux
  `~/.renderHAMgui`) und beim nächsten Start automatisch geladen. Vorrang:
  `--lang`-Kommandozeilenargument > gespeicherte Einstellung > Englisch.

**Ein echter Bug unterwegs gefunden**: `&Save As...` und das neue
`&Settings...` beanspruchten beide den Mnemonic-Buchstaben "S" –
Tastatur-Navigation landete dadurch beim falschen Menüpunkt. Behoben
durch `Se&ttings...` (Mnemonic auf "t", entsprechend Deutsch
`Ein&stellungen...`).

**Verifiziert** (Xvfb + fluxbox + `xdotool`, ohne echtes Display):
- Eigenständiger Testharness (`wxInitializer` statt vollem `wxApp`-Loop)
  bestätigt direkte Übersetzungs-Lookups: `wxGetTranslation("Open...")`
  liefert exakt `"Oeffnen..."` bei geladenem deutschen Katalog, inklusive
  korrekt aufgelöster verschachtelter Format-Strings; ein nicht im
  Katalog vorhandener String fällt sauber auf den englischen
  Quelltext zurück statt zu crashen oder leer zu bleiben.
- `msgfmt --check` bestätigt alle 60 Strings ohne Format-String-Fehler
  übersetzt.
- Settings-Dialog per Tastatur-Navigation bedient (Menü-Mnemonic bzw.
  Mausklick auf die Menüleiste, da Alt+Mnemonic vom Fenstermanager
  abgefangen wurde): Debug-Log über zwei aufeinanderfolgende
  Programmstarts bestätigt den kompletten Kreislauf – erster Start
  `current='en'`, Auswahl "Deutsch", Konfigurationsdatei
  (`Language=de`) wird geschrieben, Neustart per `wxExecute`; im neu
  gestarteten Prozess zeigt ein zweiter Durchlauf korrekt
  `current='de'`. Nur ein sauberer Prozess läuft danach weiter – der
  alte hat sich erwartungsgemäß beendet, kein Zombie-Prozess.
- Beachtenswerter Nebeneffekt: `wxLocale` stellt mit der Sprache auch
  automatisch die Dezimalschreibweise um (`0,0126` statt `0.0126` in
  der Statuszeile) – das ist Standard-Verhalten der zugrundeliegenden
  C-Locale, kein separat implementiertes Feature, und wurde bewusst
  nicht unterdrückt.

**Bekannter Nachtrag – "Öffnen"/"Speichern unter" blieben auf Deutsch
trotz englischer Oberfläche**: Die beiden Buttons (und vorsorglich auch
die gleichnamigen Menüpunkte) nutzten `wxID_OPEN`/`wxID_SAVEAS` –
erkennbare "Stock"-IDs. Auf manchen GTK-Systemen kann ein `wxButton`
mit einer solchen ID GTKs eigenes, über die C-Bibliotheks-Locale
gesteuertes Stock-Label rendern, unabhängig vom expliziten Text und
unabhängig von `wxLocale`. **In dieser Sandbox nicht reproduzierbar**
(englischer Text erschien korrekt selbst mit erzwungenem
`LANG=de_DE.UTF-8`, und es ist gar kein `wxstd.mo`-Katalog installiert,
über den der Mechanismus hier überhaupt greifen könnte) – der Fix
(eigene, nicht-Stock-IDs für beide Buttons und Menüpunkte) ist trotzdem
die dokumentierte, empfohlene Lösung für genau diese Fehlerklasse und
mit ziemlicher Sicherheit korrekt, aber **von mir nicht visuell
bestätigt**. Rückmeldung, ob es auf dem tatsächlichen System behoben
ist, wäre hilfreich.

### Gespeicherte Standardeinstellungen

Ein "Save as Defaults"-Button (neben "Convert") schreibt den
aktuellen Zustand der folgenden Einstellungen in dieselbe
`wxConfig`-Datei wie die Sprachauswahl (Linux: `~/.renderHAMgui`, unter der
Gruppe `[Defaults]`), von wo sie beim nächsten Start automatisch
geladen werden:

- Fenstergröße und -position
- Zielauflösung (Preset-Index sowie die benutzerdefinierten
  Breite/Höhe-Werte, falls "Custom..." gewählt ist)
- Skalierungs-Modus (Fit/Fill/Stretch)
- Dithering, Palette-Quantizer, Farbfokus (per Name, nicht per
  Index – robust gegenüber künftig hinzugefügten Algorithmen; fehlt
  der gespeicherte Name in der aktuellen Registry, bleibt die
  Auswahl beim eingebauten Standard)
- Verfeinerungs-Durchläufe, Farbe-vs-Helligkeit-Gewichtung
- Ausgabe-Flags (ByteRun1-Kompression, Interlace-Modus)

**Bewusst nicht** in den Standardeinstellungen enthalten: Zuschnitt
(immer bildspezifisch) und Pixel-Seitenverhältnis (war nicht in der
Anforderung genannt). Die Sprache wird separat und sofort beim Ändern
im Settings-Dialog gespeichert (siehe oben) – der "Save as
Defaults"-Button muss sie nicht zusätzlich behandeln.

Fehlt die Konfigurationsdatei (erster Start) oder einzelne Schlüssel
(z. B. nach einem Update, das eine neue Einstellung hinzufügt), bleibt
die jeweilige Kontrolle einfach beim im Code fest hinterlegten
Standard – kein Sonderfall nötig. Die Fenstergröße wird beim Laden auf
die vom Sizer berechnete Mindestgröße geklemmt, falls der gespeicherte
Wert kleiner ist (verhindert ein zu kleines, nicht mehr bedienbares
Fenster).

**Verifiziert**: Konfigurationsdatei von Hand mit bewusst von den
Standardwerten abweichenden Werten für alle zehn Einstellungen
vorbereitet (u. a. `Dither=jarvis-judice-ninke`, `Quantizer=octree`,
`FitMode=2`, `WindowWidth=1250`) – Debug-Log beim Start bestätigt
jeden einzelnen Wert korrekt übernommen. Umgekehrt: "Save as
Defaults" angeklickt und die geschriebene Datei direkt geprüft – alle
14 Schlüssel vorhanden, Werte entsprechen exakt dem aktuellen
(Standard-)Zustand inklusive der tatsächlichen Fenstergeometrie.

### Zwei kleine Anzeige-/Größen-Verbesserungen

- Der Dateiname neben "Öffnen..." zeigt jetzt zusätzlich die
  Original-Pixelauflösung des geladenen Bilds, z. B. `test_input.png
  (640x400)`. Verifiziert per Debug-Log: `input_label='test_input.png
  (640x400)'` nach dem Laden.
- Die minimale Fenstergröße (per Fenstermanager-Rand ziehbar) war
  bislang exakt an das vom Sizer berechnete "natürliche" Minimum
  (~1114×768) gekoppelt – das ist eher eine komfortable als eine
  zwingend nötige Größe, jedes Steuerelement bleibt auch deutlich
  darunter korrekt bedienbar, nur enger gepackt. Explizit auf 700×500
  heruntergesetzt. Verifiziert: Fenster per `xdotool` erzwungen auf
  genau 700×500 skaliert, danach Bildschirmposition des am weitesten
  rechts liegenden Steuerelements im Parameter-Panel (die
  ByteRun1-Kompression-Checkbox) geprüft – liegt vollständig innerhalb
  der Fenstergrenzen, kein Abschneiden. Ein Versuch, per `xdotool` auf
  400×300 zu verkleinern, wurde vom Fenstermanager korrekt auf
  ~698×473 begrenzt – die Untergrenze greift wie beabsichtigt.

### Drei Layout-Anpassungen

- **Zoom-Rahmen**: die vier Zoom-Buttons (Quelle wie Ziel) plus die
  Prozentanzeige stecken jetzt in einem beschrifteten "Zoom"-Rahmen
  (`wxStaticBoxSizer`). Neue Reihenfolge: `100%`, `Fit`, `-`, `+`
  (vorher `-`, `Fit`, `100%`, `+`). `-`/`+` sind jetzt halb so breit
  (42px statt 85px natürlicher Breite). Dabei einen durch den neuen,
  etwas breiteren Rahmen erstmals sichtbar gewordenen
  Symmetrie-Fehler gefunden: die unterschiedlich langen Titel
  ("Source" vs. "Amiga Preview (HAM6)") verzerrten die 50/50-Aufteilung
  der beiden Vorschau-Spalten um 2px (477 vs. 479px) – vorher durch
  mehr Spielraum unsichtbar. Behoben durch eine explizite
  Mindestbreite auf dem Titel-Text.
- **Crop-Rahmen**: X/Y stehen jetzt unterhalb Width/Height statt
  daneben (2×2-Raster statt 1×4).
- **"Save current Settings"**: umbenannt (vorher "Save as Defaults")
  und links ausgerichtet statt in der Mitte der Statuszeile.

**Verifiziert** per Debug-Log: Zoom-Reihenfolge und Button-Breiten
exakt bestätigt (`100%=(x=825,w=85) Fit=(x=914,w=85) -=(x=1003,w=42)
+=(x=1049,w=42)`); Crop-Raster bestätigt (`W=(268,416) H=(390,416)`
in einer Zeile, `X=(268,475) Y=(390,475)` eine Zeile darunter, X
exakt unter W, Y exakt unter H); Save-Button-Position bestätigt
(`save=(x=255)` deutlich links von `convert=(x=1030)`); beide
Vorschau-Panels nach dem Symmetrie-Fix wieder exakt `443×79` groß.

### Zielverzeichnis + Schnell-Speichern

Drei Buttons in der Werkzeugleiste, in der Reihenfolge
"Target directory...", "Save", "Save As...":

- **"Target directory..."** öffnet einen nativen Verzeichnis-Dialog
  (`wxDirDialog`). Die Auswahl wird sofort in `wxConfig` gespeichert
  (Schlüssel `TargetDirectory`, Linux: `~/.renderHAMgui`) – genau wie die
  Spracheinstellung, unabhängig von "Save current Settings". Der
  Button zeigt das aktuell gewählte Verzeichnis als Tooltip (oder
  einen Hinweis, dass noch keins gewählt wurde).
- **"Save"** speichert die aktuell geladene Datei direkt im
  Zielverzeichnis, ohne Dialog – der Original-Dateiname ohne
  Erweiterung dient als Grundlage, `.iff` wird angehängt (z. B.
  `foto.jpg` → `foto.iff`). Überschreibt bestehende Dateien ohne
  Rückfrage – das ist der Sinn von "Save" im Unterschied zu
  "Save As...". Fehlt entweder ein geladenes Bild oder ein
  Zielverzeichnis, erscheint ein Hinweis statt eines Absturzes.

**Verifiziert**: Werkzeugleisten-Reihenfolge per Debug-Log bestätigt
(`open=(x=255) target_dir=(x=800) save_quick=(x=954) save_as=(x=1047)`).
"Save" ohne gesetztes Zielverzeichnis geklickt – ein "Notice"-Fenster
erscheint korrekt, kein Absturz. Zielverzeichnis vorab in der
Konfigurationsdatei gesetzt, Programm gestartet – wird korrekt geladen
(`value='/tmp/save_target_test'`). "Save" geklickt – Datei
`test_input.iff` (aus `test_input.png`) wird im Zielverzeichnis
angelegt, besteht `netpbm`-Validierung und ist **bytegleich** mit der
CLI-Ausgabe für dieselben Parameter.

### Behobener Bug: ByteRun1-Kompression über Zeilengrenzen hinweg

Frühere Versionen komprimierten den gesamten Bitplane-Block als einen
durchgehenden ByteRun1-Strom. Das ist syntaktisch gültiges ByteRun1 und
dekomprimiert bei einem naiven (zeilen-unwissenden) Decoder korrekt –
aber reale ILBM-Reader (Amiga-Software, GIMPs IFF-Plugin, netpbms
`ilbmtoppm`) dekomprimieren **pro Zeile** und erwarten, dass kein
Kompressions-Token über eine Zeilengrenze hinausläuft. Dateien, bei
denen ein Run zufällig über eine Zeilengrenze lief, wurden von solchen
Readern abgelehnt. Betraf potenziell jede Auflösung, trat aber bei
Breiten, die keine Vielfachen von 16 sind, wegen der zusätzlichen
Padding-Nullen am Zeilenende deutlich häufiger auf.

Fix: jede Bitplane-Zeile wird jetzt einzeln komprimiert
(`byterun1_compress_planar` in `ilbm_writer.cpp`). Verifiziert mit:
1. einem eigenen, zeilen-bewussten Python-Decoder, der bei einer
   Zeilengrenzen-Überschreitung hart abbricht (`testdata/verify_row_compression.py`),
2. bit-exaktem Vergleich gegen die unkomprimierte Referenz,
3. echtem externen Decoder (`netpbm`s `ilbmtoppm`) über mehrere
   Breiten/Höhen inkl. mehrerer nicht durch 8/16 teilbarer Werte und
   Extremfällen wie 1×1.

## Palette sperren, speichern, laden; Farbe 0 fixieren

Vorbereitend für eine später geplante ANIM-Fähigkeit (siehe Diskussion
weiter oben in der Projekthistorie: eine über mehrere Frames hinweg
konstante Palette vermeidet Farbverschiebungen zwischen Frames, die
sonst durch pro Bild unabhängig neu optimierte Paletten entstünden).

- **`--save-palette <pfad.iff>`**: speichert die von dieser Konvertierung
  tatsächlich verwendete Palette (egal ob frisch quantisiert oder
  gesperrt) als eigenständige Datei. Bewusst **kein** eigenes Format:
  `save_palette()` baut ein triviales 1×1-`HAM6Frame` mit genau dieser
  Palette und schreibt es über denselben `write_ham6_ilbm()`, den auch
  die echte Ausgabe nutzt – das Ergebnis ist ein vollwertiges,
  eigenständig ladbares ILBM (ein echter Amiga-Betrachter zeigt einfach
  ein 1×1-Bild, die Palette ist trotzdem vollständig vorhanden und
  besteht die `netpbm`-Validierung).
- **`--load-palette <pfad.iff>`**: lädt die Palette aus der CMAP-Chunk
  einer IFF-ILBM-Datei (der eigenen `--save-palette`-Ausgabe, oder
  grundsätzlich jeder echten Amiga-ILBM-Datei mit 16-Farben-Palette)
  und **sperrt** sie – die Quantisierung wird komplett übersprungen,
  `--palette-iterations` wird ignoriert (wie `0` behandelt), da ein
  Nachjustieren einer bewusst fixierten Palette deren Zweck zuwiderliefe.
- **`--fix-color0 <r>,<g>,<b>`** (8-Bit-Werte, z. B. `0,0,0`): erzwingt
  Palette-Index 0 auf einen festen Wert – sowohl in der Saat-Palette als
  auch nach jedem einzelnen Verfeinerungsdurchlauf neu, damit Farbe 0
  nie driftet und die übrigen 15 Farben korrekt um den endgültigen
  Wert herum passen, nicht um einen Platzhalter, der erst am Ende
  überschrieben wird. Relevant, weil Farbe 0 auf echter Amiga-Hardware
  zugleich die Rand-/Hintergrundfarbe außerhalb der eigentlichen
  Bitmap-Fläche ist. Funktioniert unabhängig von und in Kombination mit
  `--load-palette`.

**Verifiziert**:
- Gespeicherte Palettendatei besteht `netpbm`-Validierung (`ilbmtoppm`)
- **Wichtigster Nachweis für `--load-palette`**: Palette von einem
  *anderen* Testbild geladen und auf `test_input.png` angewendet –
  Fehler steigt messbar von `0,0126422` (eigene Palette) auf `0,018131`
  (fremde, schlechter passende Palette) – belegt, dass der
  Paletteninhalt tatsächlich verwendet wird, nicht nur eine
  entgegengenommene, aber ignorierte Option ist
  - `--fix-color0` direkt in der gespeicherten Datei nachgeprüft: Farbe
  0 exakt wie angefordert (z. B. `0,0,0` → `(0,0,0)`, `255,0,0` →
  `(255,0,0)`), die übrigen 15 Farben weiterhin normal variiert
- Fehlerfälle geprüft: fehlende Datei, Nicht-ILBM-Datei (z. B. ein PNG),
  falsches `--fix-color0`-Argumentformat – jeweils klare Fehlermeldung,
  Exit-Code 1
- Kombination `--load-palette` + `--fix-color0` gleichzeitig: geladene
  Palette bleibt Basis, Index 0 wird trotzdem korrekt überschrieben
- CLI-Baseline ohne die neuen Flags unverändert (`0,0126422`)

**GUI-Nachtrag**: eigene Box "Palette" (zwischen "HAM conversion
settings" und "Output"). "Load Palette..."/"Save Palette..."-Buttons
rufen dieselben `aic::load_palette`/`save_palette`-Funktionen wie die
CLI auf. Die Checkbox "Lock Palette" ist zunächst deaktiviert und wird
erst aktiv, sobald **entweder** eine Palette geladen wurde **oder**
mindestens eine Konvertierung gelaufen ist – `ParameterPanel` hält dafür
eine `reference_palette_`, die `MainFrame` nach jeder erfolgreichen
Konvertierung über `NotifyPaletteGenerated()` aktualisiert (das jeweils
zuletzt eingetretene Ereignis – Laden oder Konvertieren – gewinnt).
"Fix color 0" nutzt `wxColourPickerCtrl` (Farbwähler-Button mit
eingebautem RGB-Dialog, deckt "editierbar als RGB-Wert oder per
Farbwähler" mit einem Widget ab) und persistiert über "Save current
Settings"/`LoadDefaults` – "Lock Palette" bewusst **nicht**, da eine
gemerkte Checkbox-Markierung ohne die zugehörige, nicht persistierte
Referenzpalette beim nächsten Start bedeutungslos wäre.

**Verifiziert** per Debug-Log (Xvfb + fluxbox + xdotool):
- Zustandskette vollständig bestätigt: vor jeder Konvertierung
  deaktiviert (`lock_enabled=0 has_ref=0`) → nach der ersten
  erfolgreichen Konvertierung aktiviert (`has_ref=1`) → nach Anhaken
  tatsächlich in die Optionen übernommen (`locked_in_opts=1`)
- **Wichtigster Test für Fix Color 0** (nicht nur mit der Standardfarbe
  Schwarz, die zufällig auch das natürliche Ergebnis ist, sondern mit
  einer eindeutig unterscheidbaren Farbe): Konfigurationsdatei mit
  `FixColor0R/G/B=200,30,90` vorbereitet, Programm gestartet – die
  tatsächlich erzeugte Palette zeigt exakt `r4=12 g4=2 b4=5`,
  übereinstimmend mit der erwarteten 4-Bit-Rundung – belegt die
  komplette Kette von der Konfigurationsdatei über den Farbwähler bis
  zum tatsächlichen Encoder-Ergebnis
- "Save current Settings" mit angehakter Fix-Color0-Checkbox geklickt:
  Konfiguration zeigt korrekt `FixColor0Enabled=1`, `FixColor0R/G/B=0,0,0`
- "Load Palette..."/"Save Palette..." angeklickt: beide öffnen den
  jeweils richtig betitelten nativen Dateidialog ("Load Palette" bzw.
  "Save Palette As"), kein Absturz – die zugrundeliegenden
  Lese-/Schreibfunktionen sind bereits über die CLI erschöpfend
  geprüft (siehe oben)

**Nachtrag – korrigiert: nicht mehr 16 einzelne Fix-Checkboxen, sondern
Lock Palette (alle 16) plus ein einzelnes Lock für Farbe 0:**

Die vorherige Fassung (16 individuelle "Fix color N"-Checkboxen) ging
über das eigentlich Gewünschte hinaus und wurde auf ausdrücklichen
Wunsch zurückgebaut. Das jetzige, korrigierte Verhalten:

- **"Load Palette..."** ist immer bedienbar (war bereits so).
- **"Save Palette..."** ist nur aktiv, wenn bereits eine Palette
  existiert – geladen oder aus der letzten Konvertierung erzeugt (war
  bereits so). Speichert dabei den *aktuellen, live editierbaren*
  Zustand der 16 Farbfelder, nicht die ursprünglich erzeugte/geladene
  Momentaufnahme.
- **"Lock Palette"** behält die aktuelle Palette – gelesen direkt aus
  den 16 Farbfeldern, nicht aus einer gespeicherten Momentaufnahme –
  vollständig und verwendet sie unverändert bei der nächsten
  Konvertierung; es wird dann keine eigene Palette berechnet
  (Quantisierung und Verfeinerung entfallen komplett).
- **Nur Farbe 0 bekommt eine eigene, zusätzliche "Lock color 0
  (Amiga background)"-Checkbox**, da diese Farbe auf echter
  Amiga-Hardware zugleich die Hintergrundfarbe darstellt. Ist diese
  Checkbox aktiv und "Lock Palette" *nicht*, wird das bei der
  Palettenerzeugung berücksichtigt: nicht alle 16 Grundfarben werden
  neu optimiert, sondern nur Farbe 1 bis 15 – Farbe 0 bleibt exakt auf
  dem im Farbfeld gezeigten Wert fixiert. Ist "Lock Palette"
  gleichzeitig aktiv, hat Vorrang – "Lock color 0" wird dann
  gegenstandslos, da ohnehin die komplette Palette unverändert bleibt.
- Alle 16 Farbfelder sind **immer** bedienbar, unabhängig vom
  Sperrzustand – auch bei aktivem "Lock Palette" lässt sich jede
  einzelne Farbe direkt anpassen, und diese Änderung wirkt sich sofort
  auf die nächste Konvertierung aus.
- Nach jeder Neugenerierung einer Palette (Konvertierung) oder nach
  dem Laden einer Palette werden alle 16 Farbfelder automatisch mit
  den tatsächlichen, aktuellen Werten synchronisiert
  (`PopulateColorPickers()`, gemeinsam genutzt von
  `NotifyPaletteGenerated` und `OnLoadPalette`).
- Die Farbfelder selbst sind jetzt kleiner und annähernd quadratisch
  (28×28 Pixel, ohne die sonst übliche Textbeschriftung des
  Farbwähler-Widgets) statt der vorherigen breiten Variante –
  zusammen mit dem Wegfall der 16 Einzel-Checkboxen macht das
  Gesamtlayout wieder spürbar kompakter als in der vorherigen,
  zurückgebauten Fassung.

Bibliotheksseitig bleibt die in der vorherigen Fassung eingeführte
Verallgemeinerung (`fixed_colors`, ein 16-elementiges Array in
`HAM6EncoderOptions`/`ConversionOptions`) unverändert bestehen – sie
wird nur anders angesteuert: die GUI setzt jetzt gezielt nur noch
Index 0 (über "Lock color 0"), nie mehr alle 16 einzeln. Die CLI-Flag
`--fix-color0` bleibt unverändert nutzbar.

**Verifiziert** (Debug-Hotkeys, da einzelne Checkbox-Klicks in dieser
Sandbox wiederholt unzuverlässig registrierten): eine saubere
4-Schritt-Sequenz bestätigt exakt die spezifizierte Rangfolge –
(1) keine Sperre aktiv → weder `locked_palette` noch `fixed_colors[0]`
gesetzt; (2) nur "Lock Palette" aktiv → `locked_palette` korrekt aus
den *aktuellen* Farbfeld-Werten aufgebaut; (3) nur "Lock color 0"
aktiv → ausschließlich `fixed_colors[0]` gesetzt, `locked_palette`
bleibt leer; (4) beide gleichzeitig aktiv, nachdem Farbe 0 und 1 zuvor
direkt in den Farbfeldern auf neue, unterscheidbare Werte geändert
wurden → "Lock Palette" gewinnt wie spezifiziert, und die gesperrte
Palette enthält exakt die frisch bearbeiteten Werte – bestätigt sowohl
den Vorrang als auch, dass Bearbeitung bei aktivem "Lock Palette"
tatsächlich wirkt. Zusätzlich per Screenshot bestätigt: "Current
palette" zeigt nach einer normalen Konvertierung sofort die
tatsächlich berechnete, variierende 16-Farben-Palette (nicht mehr das
Standard-Schwarz aller Felder). CLI-Baseline unverändert (`0,0126422`),
Release-Build sauber (0 Warnungen), Präprozessor-Direktiven nach
Debug-Code-Entfernung auf Balance geprüft (0 ifdef / 0 endif).

## Animations-Keyframes und Serien-Export (nur GUI)

Allererster Baustein einer später geplanten Animationsfähigkeit (Ken
Burns-artiger Schwenk/Zoom über ein Standbild, siehe die Diskussion zur
Machbarkeit in der Projekthistorie): im "Crop"-Bereich lässt sich für
mehrere **Keyframes** jeweils ein eigener Zuschnittsbereich festlegen.
Interpolation zwischen den Keyframes und tatsächliche Bild-Generierung
sind bewusst noch **nicht** Teil dieses Schritts – hier geht es nur
darum, mehrere benannte Zuschnittsbereiche anzulegen und zu bearbeiten.

- **"Add Keyframe"**: übernimmt den aktuell eingestellten
  Zuschnittsbereich als neuen, eigenständigen Eintrag ("Keyframe 1",
  "Keyframe 2", …), ohne ihn auszuwählen (siehe Nachtrag weiter unten
  zu einem daraus entstandenen Fehler und dessen Korrektur).
  Deaktiviert, solange Zuschnitt nicht aktiviert ist (wie die übrigen
  Zuschnitt-Regler).
- **Dropdown-Auswahl**: Wechseln zu einem vorhandenen Keyframe lädt
  dessen gespeicherten Bereich in die Zahlenfelder/das Rubber-Band.
- **Bearbeiten bei aktiver Auswahl**: jede Änderung am Zuschnittsbereich
  (Zahlenfelder, Rubber-Band-Ziehen, Seitenverhältnis-Sperre) schreibt
  automatisch in das gerade ausgewählte Keyframe zurück –
  `NotifyCropChanged()` ist der zentrale Punkt, durch den ausnahmslos
  alle Änderungswege laufen, daher genügt eine einzige Stelle für das
  Zurückschreiben.
- **"Remove"**: entfernt das ausgewählte Keyframe, Beschriftungen der
  übrigen werden neu durchnummeriert.

Ein `const std::vector<wxRect>& GetCropKeyframes() const` liegt bereit
und wird inzwischen tatsächlich verwendet – von `MainFrame::RunAnimationExport`,
siehe den Nachtrag weiter unten.

**Verifiziert**: Kernmechanismus per Debug-Log klar bestätigt – ein
Keyframe ändert sich nachweislich von `(0,0,100,100)` auf
`(0,9,100,100)`, nachdem während dessen Auswahl ein Wert bearbeitet
wurde; mehrere Keyframes bleiben dabei unabhängig voneinander
gespeichert (ein zweites, danach hinzugefügtes Keyframe beeinflusst
das erste nicht rückwirkend). Aktivierung/Deaktivierung der
Steuerelemente korrekt an den Zuschnitt-Status gekoppelt.

Für "Remove" ließ sich der reine Button-Klick in dieser
Sandbox-Umgebung trotz zahlreicher Versuche (verschiedene Koordinaten,
großzügige Wartezeiten, exaktes Button-Zentrum anhand gemessener
Größe, explizites mousedown/mouseup, Doppelklick) nicht zuverlässig
per `xdotool` auslösen, obwohl ein Screenshot die Button-Position
visuell bestätigte – eine umgebungsspezifische Eigenheit dieser
Xvfb/Fluxbox-Sitzung, kein erkennbarer Fehler in Koordinaten oder
Layout. Stattdessen direkt bewiesen: ein temporärer Debug-Hotkey rief
denselben Produktions-Handler (`OnRemoveKeyframe`) auf, den ein echter
Klick auslösen würde, unter Umgehung jeder Koordinaten-Unsicherheit –
Ergebnis korrekt: `count=2→1`, `selection` korrekt auf "keine
Auswahl" zurückgesetzt, "Remove"-Button danach korrekt deaktiviert.

**Nachtrag – zwei vom Nutzer gemeldete Fehler behoben:**

1. **Rubber-Band aktualisierte sich beim Keyframe-Wechsel nicht.**
   `OnKeyframeSelected` rief `SetCropPixelRect()` auf (setzt nur die
   Zahlenfeld-Werte, löst kein Ereignis aus), aber nie
   `NotifyCropChanged()` – dadurch erfuhr die Bildschirm-Darstellung
   nie vom Wechsel. Ein Einzeiler behoben: `NotifyCropChanged()` direkt
   nach dem Laden ergänzt.

2. **Anfasser oben/links verschoben statt zu skalieren.** Ursache:
   `wxRect::SetTop()`/`SetLeft()` setzen nur `y`/`x` direkt, OHNE
   Höhe/Breite anzupassen – anders als `SetBottom()`/`SetRight()`, die
   aus dem Zielwert die Größe neu berechnen (da Unten/Rechts keine
   gespeicherten Felder sind, sondern aus Ursprung+Größe abgeleitet
   werden). Ein `SetTop()`-Aufruf allein verschiebt das Rechteck daher
   schlicht senkrecht – die untere Kante wandert unbemerkt mit statt
   fest zu bleiben. Behoben, indem vor dem Verschieben der nahen Kante
   die gegenüberliegende Kante zwischengespeichert und danach über
   `SetBottom()`/`SetRight()` wiederhergestellt wird (das berechnet
   dann korrekt die neue Höhe/Breite gegen den bereits verschobenen
   Ursprung). Betraf alle vier Fälle, die N oder W involvieren: N, W,
   NW, NE (nur die N-Komponente), SW (nur die W-Komponente). Die
   ratio-gesperrte Zuschnitt-Variante (`ResizeWithLockedRatio`) baut
   das Rechteck ohnehin komplett neu aus Anker+Zielgröße auf und war
   nie betroffen.

   Die Resize-Logik wurde dafür in eine eigene, pur funktionale
   Methode `ResizeFreeform(handle, start, dx, dy)` ausgelagert (reine
   Rechteck-Arithmetik, keine Fensterzustand-Abhängigkeit) und mit
   einem eigenständigen Testprogramm ohne jede Maussimulation direkt
   geprüft: vor der Korrektur bewegten N- und W-Drags das komplette
   Rechteck (Gegenseite verschob sich mit); nach der Korrektur halten
   alle acht Anfasser (N, S, E, W, NE, NW, SE, SW) korrekt ihre
   jeweilige Gegenseite fest. Ergänzend ein Lauf der echten Anwendung
   unter Xvfb ohne neue Warnungen oder Abstürze.

**Nachtrag – eigene "Animation"-Box, Serien-Export beim Speichern:**

Die Keyframe-Bedienelemente sind aus der Crop-Box in eine eigene Box
"Animation" umgezogen, mit einer neuen Checkbox **"Enable Animation"**
als Hauptschalter: deaktiviert bleiben Keyframe-Liste, "Add
Keyframe"/"Remove" und der neue Regler "Frames between keyframes"
ausgegraut (`UpdateKeyframeControlsEnabled` verlangt jetzt sowohl
Zuschnitt **als auch** Animation aktiviert). Ist die Checkbox aktiv,
erzeugen "Save" und "Save As..." statt einer einzelnen Datei eine
**durchnummerierte Bildfolge** (`MainFrame::RunAnimationExport`):

- Linear interpolierte Zuschnittsbereiche zwischen je zwei
  aufeinanderfolgenden Keyframes, `frames_per_segment` Bilder pro
  Segment plus ein letztes Bild exakt auf dem letzten Keyframe (die
  Segment-Schleife selbst stoppt bewusst kurz vor t=1, um das
  gemeinsame Keyframe zwischen zwei Segmenten nicht doppelt zu
  erzeugen).
- Dateinamen `<ursprünglicher Name>_0001.iff`, `..._0002.iff`, …,
  4-stellig nullgefüllt, im selben Verzeichnis wie der gewählte
  Zielpfad.
- **Palette**: Bild 1 verwendet, was `GetOptions()` ohnehin liefert –
  eine bereits aktive gesperrte Palette (Lock Palette) oder eine
  frische Quantisierung. Die tatsächlich dabei entstandene Palette
  wird danach für **alle** weiteren Bilder fest übernommen
  (`opts.locked_palette`) – das deckt beide vom Nutzer beschriebenen
  Fälle ab, ohne dass der Code zwischen ihnen unterscheiden muss: war
  Lock Palette schon aktiv, IST Bild 1s Ergebnis bereits diese
  gesperrte Palette; war sie es nicht, wird Bild 1s frisches Ergebnis
  zur gemeinsamen Palette. Nach Abschluss wird diese Palette zusätzlich
  über `NotifyPaletteGenerated` als "zuletzt erzeugte Palette"
  hinterlegt (aktiviert "Lock Palette" für künftige Arbeit, ohne sie
  zwangsweise anzuhaken).
- **Live-Fortschritt**: nach jedem geschriebenen Bild wird
  `last_preview_` aktualisiert und `UpdatePreviewDisplay()` sowie
  `wxYield()` aufgerufen – die Amiga-Vorschau zeigt dadurch während der
  Erzeugung laufend das zuletzt geschriebene Bild, statt erst am Ende
  aufzuwachen. Statuszeile zählt "Generating animation: Frame N/M..."
  mit.
- Eine echte Amiga-ANIM-Datei (mit Delta-Kompression) ist bewusst noch
  **nicht** Teil dieses Schritts, ebenso wenig eine automatisch
  optimierte, über alle Frames hinweg berechnete Palette – beides
  mögliche spätere Ausbaustufen.

**Workflow-Fehler gefunden, zwischenzeitlich falsch behoben, jetzt
endgültig korrekt**: "Add Keyframe" wählte den neu erzeugten Eintrag
zunächst automatisch aus. Da eine Auswahl bedeutet, dass weitere
Zuschnitt-Änderungen automatisch in genau dieses Keyframe
zurückgeschrieben werden (siehe oben), machte das es unmöglich, einen
zweiten, abweichenden Keyframe vorzubereiten, ohne den ersten dabei zu
überschreiben. Zunächst abgeschaltet, dann auf ausdrücklichen Wunsch
wieder aktiviert (mit einem dokumentierten, aber in der Praxis nicht
gangbaren "korrekten Ablauf") – und genau das führte zu einem vom
Nutzer gemeldeten Fehler beim Animations-Export: bei zwei Keyframes
mit unterschiedlichen Zuschnittsbereichen ging der Bereich eines der
beiden verloren. Per Debug-Hotkey exakt reproduziert: "Add Keyframe"
(Keyframe 1 = 100, automatisch ausgewählt) → Breite auf 150 geändert
(schreibt, da noch ausgewählt, in Keyframe 1 zurück – Keyframe 1 wird
dadurch selbst zu 150) → "Add Keyframe" erneut → **beide** Keyframes
zeigen 150, der ursprüngliche Wert 100 ist komplett verloren. Die
automatische Auswahl und das Zurückschreiben bei Bearbeitung eines
ausgewählten Keyframes sind für den naheliegenden Arbeitsablauf
(Position → Add → Position ändern → Add) grundsätzlich unvereinbar,
unabhängig davon, wie der Ablauf dokumentiert wird.

Endgültig behoben durch Rückbau auf die ursprüngliche, nachweislich
fehlerfreie Variante: "Add Keyframe" wählt den neuen Eintrag **nicht**
mehr automatisch aus. Erst ein bewusstes Auswählen aus der Liste
aktiviert die Live-Bearbeitung für ein bestimmtes Keyframe. Der
naheliegende Arbeitsablauf (Position einstellen → "Add Keyframe" →
Position ändern → "Add Keyframe" → …) funktioniert dadurch ohne
Fallstrick, weil zwischen zwei "Add"-Klicks nichts ausgewählt ist und
Positionsänderungen dazwischen folglich in keinem bestehenden Keyframe
landen.

**Verifiziert** (Debug-Hotkeys, da einzelne Klicks in dieser Sandbox
wiederholt unzuverlässig registrierten): dieselbe Fehler-Sequenz nach
dem Fix wiederholt – "Add Keyframe" (Keyframe 1 = 100, nicht
ausgewählt) → Breite auf 150 geändert (Keyframe 1 bleibt unverändert
bei 100, da nichts ausgewählt ist) → "Add Keyframe" erneut → Keyframe
1 = 100 (erhalten!), Keyframe 2 = 150 – zwei tatsächlich
unterschiedliche Keyframes. Zusätzlich bestätigt, dass explizites
Auswählen eines Keyframes aus der Liste weiterhin korrekt
funktioniert: Keyframe 1 explizit ausgewählt, Breite geändert →
ausschließlich Keyframe 1 aktualisiert sich, Keyframe 2 bleibt
unangetastet.

**Verifiziert** (Xvfb + fluxbox + xdotool, ursprünglicher Animations-Export-Test):
- 2 Keyframes, 10 Frames/Segment → korrekt 11 Dateien, 4-stellig
  nullgefüllt, alle bestehen `netpbm`-Validierung
- Bildinhalt unterscheidet sich tatsächlich zwischen Frame 1, 6 und 11
  (Zuschnitt interpoliert nachweislich, nicht nur dem Namen nach)
- Beide Paletten-Fälle einzeln beweisen: ohne vorherige Sperre teilen
  sich alle 11 Bilder exakt Bild 1s frisch quantisierte Palette;
  mit vorher aktivierter Sperre (Referenzpalette aus einer separaten
  CLI-Konvertierung erzeugt und verglichen) übernehmen alle 11 Bilder
  exakt diese gesperrte Palette, byteweise identisch
- Statuszeile zeigt nach Abschluss korrekt "Animation exported: 11
  frames to …", Amiga-Vorschau zeigt sichtbar das zuletzt erzeugte Bild
- CLI-Baseline unverändert (`0,0126422`), Release-Build sauber, breiter
  GUI-Sanity-Lauf ohne neue Warnungen

**Nachtrag – echte, delta-komprimierte Amiga-IFF-ANIM-Datei:**

In der "Animation"-Box zusätzliche Radiobuttons **"Output"**: "Single
Images" (bisheriges Verhalten, nummerierte Einzeldateien) oder
**"Anim-File"** – dann erzeugt "Save"/"Save As..." statt der
Bildfolge eine einzige `<name>.anim`-Datei im echten Amiga-IFF-ANIM-Format
(Methode 5, "Byte Vertical Delta", entwickelt von Jim Kent – der
verbreitetste, am besten unterstützte ANIM-Kompressionsmodus). Neues
Bibliotheksmodul `anim_writer.hpp/cpp`, implementiert nach der
originalen IFF-ANIM-Spezifikation (Gary Bonham/Sparta, 1998):

- Frame 0 ist ein normales ILBM (BMHD/CMAP/CAMG/BODY, ByteRun1-komprimiert
  wie gehabt). Ab Frame 2 folgt statt BMHD/BODY ein ANHD (40 Byte
  fester Header: Operation, Timing in Jiffies, u. a.) plus DLTA-Chunk
  mit den kodierten Unterschieden zur Referenz.
- **Doppelpufferungs-Schema** (Interleave=0, von praktisch allen
  echten Amiga-Playern unterstützt, im Gegensatz zur selteneren
  Interleave=1-Variante mancher Brush-Werkzeuge): Frame 2 referenziert
  Frame 1, ab Frame 3 referenziert jedes Frame das jeweils **zwei**
  Frames zurückliegende (nicht das unmittelbar vorherige) – exakt wie
  in der Originalspezifikation beschrieben.
- Kodierung pro Bitplane und Byte-Spalte (eine feste X-Byte-Position,
  über alle Zeilen hinweg durchlaufen): unveränderte Zeilenläufe werden
  als "Skip" übersprungen, geänderte als "Uniq" mit den tatsächlichen
  neuen Bytes gespeichert. Der optionale, kompaktere "Same"-Opcode
  (eine Byte-Wiederholung) wird von diesem Encoder bewusst nie erzeugt
  – Korrektheit vor Kompressionsgrad für eine erste Umsetzung; ein
  Decoder, der "Same" versteht, dekodiert reines Skip/Uniq trotzdem
  einwandfrei, da "Same" nur eine alternative, kompaktere Kodierung
  für einen Fall ist, den Uniq bereits abdeckt.
- Da der Opcode-Zähler pro Spalte in einem einzigen Byte steht (max.
  255), bricht die Kodierung mit einer klaren Fehlermeldung ab, falls
  eine Spalte zwischen zwei Frames öfter als 255-mal wechselt – in der
  Praxis für Schwenk-Animationen (viel gemeinsamer Bildinhalt
  zwischen benachbarten Frames) nicht zu erwarten.

**Verifiziert** – der wichtigste Nachweis war ein komplett unabhängig
geschriebener Decoder (eigene ByteRun1-Dekompression, eigene
Delta-Dekodierung, eigene Chunky-zu-Planar-Umwandlung, bewusst *ohne*
denselben Code wie der Encoder zu verwenden), der jedes Frame aus der
geschriebenen Datei rekonstruiert und byteweise mit den
Original-Bitplane-Daten vergleicht:
- 4 Testframes (64×48) sowie 7 Testframes bei realistischer
  320×256-Auflösung – **alle Frames exakt byteidentisch** rekonstruiert,
  inklusive der korrekten "zwei Frames zurück"-Referenzlogik über
  mehrere Iterationen hinweg
- Größenvergleich gegen einzelne Dateien: bei langsamem, realistischem
  Schwenk (1,5 Pixel/Frame) rund 5 % kleiner als die Summe der
  Einzeldateien – moderat, aber erwartbar bei gedithertem
  HAM6-Bildmaterial, das naturgemäß weniger exakt wiederkehrende Bytes
  liefert als z. B. flächige Grafik
- Echte, aus der GUI heraus mit zwei Keyframes erzeugte `.anim`-Datei
  strukturell geprüft: gültiger `FORM ANIM`-Header, genau 11
  verschachtelte `FORM ILBM`-Chunks (2 Keyframes × 10 Frames/Segment + 1,
  wie erwartet)
- Für die Radiobutton-Auswahl selbst galt dieselbe
  Klick-Zuverlässigkeits-Einschränkung dieser Sandbox-Umgebung wie
  zuvor beim "Remove"-Button (siehe oben) – über einen temporären
  Debug-Hotkey direkt bestätigt: Auswahl korrekt gelesen
  (`IsAnimFileOutput()`), "Save" erzeugt daraufhin korrekt eine
  einzelne `.anim`-Datei statt der Bildfolge
- CLI-Baseline unverändert, Release-Build sauber, ILBM-Writer-Verhalten
  nach der internen Refaktorierung (gemeinsame IFF-Hilfsfunktionen für
  ILBM- und ANIM-Writer) unverändert bestätigt

**Nachtrag – zweiter, andersartiger Fehler beim Keyframe-Zurückschreiben
gefunden und behoben:** Nach dem vorherigen Fix (Auto-Auswahl bei "Add
Keyframe" zurückgebaut) meldete der Nutzer denselben Symptomtyp erneut,
diesmal über einen anderen Weg reproduziert: zwei Keyframes anlegen,
beide einmal der Reihe nach auswählen (zeigen korrekt ihren jeweiligen
Bereich), dann bei ausgewähltem zweiten Keyframe den Zuschnitt über das
**Rubber-Band im Quellbild** (nicht über die Zahlenfelder) ändern –
beim erneuten Auswählen dieses Keyframes ist die Rubber-Band-Änderung
wieder verschwunden, es erscheint wieder der alte Wert.

Ursache: `MainFrame::OnCropOverlayEdited()` (der Rückkanal vom
Rubber-Band zu den Zuschnittsfeldern) rief nach dem Aktualisieren der
Zahlenfelder (`SetCropPixelRect()`) nie das für das Zurückschreiben in
ein ausgewähltes Keyframe zuständige `NotifyCropChanged()` auf – anders
als jeder andere Weg, den Zuschnitt zu ändern (Zahlenfelder,
Seitenverhältnis-Sperre, Keyframe-Auswahl selbst), die das alle
korrekt tun. Die Rubber-Band-Änderung war dadurch nur *visuell*
sichtbar, wurde aber nie tatsächlich gespeichert – die nächste
Keyframe-Auswahl überschrieb sie wieder mit dem zuletzt gespeicherten
Wert. `NotifyCropChanged()` war zudem bislang privat und musste dafür
öffentlich gemacht werden; die bereits vorhandene `syncing_crop_`-Sperre
schützt zuverlässig vor der Rückkopplungsschleife, die ursprünglich
gegen genau diese Verbindung sprach (ein veralteter Kommentar an
`SetCropPixelRect()` behauptete das noch, wurde korrigiert).

**Verifiziert** (Debug-Hotkeys, die den echten `OnCropOverlayEdited()`-Pfad
aufrufen statt ihn nachzubilden – so wie es das Rubber-Band selbst
tut): exakte Nutzer-Sequenz nachgestellt – Keyframe 1 ausgewählt
(100×100), Keyframe 2 ausgewählt (100×100), simulierte
Rubber-Band-Änderung auf 200×200 während Keyframe 2 ausgewählt bleibt
→ Keyframe 2 korrekt aktualisiert, Keyframe 1 unverändert; Keyframe 1
erneut ausgewählt → weiterhin korrekt 100×100; Keyframe 2 erneut
ausgewählt → **bleibt korrekt bei 200×200** statt wie zuvor auf
100×100 zurückzufallen. Präprozessor-Direktiven nach
Debug-Code-Entfernung auf Balance geprüft (0 ifdef / 0 endif),
CLI-Baseline unverändert, Release-Build sauber (0 Warnungen), breiter
GUI-Sanity-Lauf ohne neue Warnungen.

### Echtzeit-Vorschau der Animation (Play Preview)

Auf Wunsch eine Live-Vorschau der interpolierten Keyframe-Sequenz,
mit "Dauerschleife" und "Vor und zurück" (Ping-Pong), frei
kombinierbar.

**Designentscheidungen (wie erbeten vorab durchdacht):**
- **Wiederverwendung des bestehenden "Amiga Preview"-Panels** statt
  eines eigenen Fensters: passt zum bisherigen Single-Window-Ansatz
  der App, und das Panel zeigt bereits seit dem Export-Feature
  Live-Fortschritt einzelner Animationsbilder – die Vorschau ist
  also nur ein weiterer, timer-statt-export-loop-getriebener Nutzer
  desselben, bereits etablierten Anzeigewegs (`UpdatePreviewDisplay()`).
  Zoom und "Simulate interlace" wirken dadurch automatisch auch
  während der Wiedergabe.
- **Alle Frames werden vollständig im Speicher vorab berechnet**
  (`StartAnimationPreview()`, seit A4 als Hintergrund-Job), nicht erst beim jeweiligen
  Timer-Tick – jeder Tick zeigt dadurch nur ein bereits fertiges
  Ergebnis an, ohne während der Wiedergabe selbst noch die Pipeline
  durchlaufen zu müssen. Dieselbe "Palette von Bild 1 wird für alle
  folgenden gesperrt"-Logik wie beim echten Export sorgt dafür, dass
  die Vorschau repräsentativ für das tatsächliche Exportergebnis ist.
- **Konfliktvermeidung mit bestehenden Abläufen**: eine laufende
  Vorschau wird automatisch gestoppt bei Convert (F5), beim echten
  Export (Save/Export Animation), beim Laden eines neuen Bildes,
  sowie – über die bereits bestehenden Callback-Mechanismen
  (`SetCropChangedCallback`, `SetAnimationToggledCallback`) und zwei
  neue nach demselben Muster (`SetPlayPreviewToggledCallback`,
  `SetAnimationCompositionChangedCallback`) – bei jeder
  Crop-/Keyframe-Änderung, jedem Umschalten von "Enable Animation"
  sowie beim Hinzufügen/Entfernen eines Keyframes oder Ändern von
  "Frames between keyframes". Der Play/Stop-Button beschriftet sich
  passend um; keine zusätzliche Statusmeldung nötig.
- "Loop" und "Vor und zurück" werden bei **jedem** Tick neu abgefragt
  (nicht nur beim Start), Umschalten während der Wiedergabe wirkt
  also sofort, ohne Stopp/Neustart.

**Ein während der Entwicklung gefundener und behobener echter Fehler**:
Der `wxTimer` wurde mit seinem Standardkonstruktor angelegt (kein
Owner gesetzt) und die Events zunächst über `this->Bind(wxEVT_TIMER,
..., preview_timer_.GetId())` auf dem Frame gebunden – das greift
aber nur, wenn dem Timer explizit ein Owner zugewiesen wurde,
andernfalls sendet er seine Events an sich selbst. Behoben durch
direktes Binden auf das Timer-Objekt selbst
(`preview_timer_.Bind(wxEVT_TIMER, &MainFrame::OnPreviewTimer,
this);`), was unabhängig von der Owner-Konfiguration immer korrekt
ist.

**Verifiziert** (Debug-Hotkeys, da einzelne Klicks in dieser Sandbox
wiederholt unzuverlässig registrierten):
- Alle vier Wiedergabe-Modi einzeln durchgerechnet und per Debug-Log
  bestätigt, mit zwei tatsächlich unterschiedlichen Keyframes (2
  Keyframes × 10 Frames/Segment + 1 = 11 Bilder):
  - Ohne Loop, ohne Ping-Pong: läuft `1→2→…→10`, stoppt danach von
    selbst (6 Sekunden Wartezeit ohne weitere Ticks bestätigt)
  - Nur Loop: läuft über mehrere vollständige Zyklen
    `1→…→10→0→1→…→10→0→…` ohne zu stoppen
  - Nur Ping-Pong (ohne Loop): läuft `1→…→10→9→…→0`, stoppt danach
    von selbst
  - Loop + Ping-Pong: läuft `1→…→10→9→…→0→1→…` über mehrere
    vollständige Hin- und Rückläufe hinweg endlos weiter
- Konfliktvermeidung einzeln bestätigt: F5 während laufender
  Dauerschleife gedrückt → Vorschau stoppt sofort, keine weiteren
  Ticks über 3 Sekunden Wartezeit; erneutes Hinzufügen von Keyframes
  während laufender Dauerschleife → Vorschau stoppt ebenfalls sofort
- Layout per Screenshot bestätigt: kompakter neuer "Preview"-Bereich
  (Play-Preview-Button, "Loop"/"Back and forth" nebeneinander wie
  beim bewährten "Pixels"/"Percent"-Muster), alle Felder korrekt
  deaktiviert, solange Animation nicht aktiv ist
- Präprozessor-Direktiven nach Debug-Code-Entfernung auf Balance
  geprüft (0 ifdef / 0 endif), CLI-Baseline unverändert (`0,0126422`),
  Release-Build sauber (0 Warnungen), breiter GUI-Sanity-Lauf ohne
  neue Warnungen

### Layout-Anpassungen: horizontale Radiogruppen, eigene Preview-Box, zwei Umbenennungen

Auf konkreten Wunsch fünf gezielte Layout- und Beschriftungsänderungen:

- **"Export format"**, **"Scaling mode"** und **"Interlace (LACE)"**
  (jetzt umbenannt, siehe unten) standen bisher als `wxRadioBox` mit
  `wxRA_SPECIFY_COLS, 1` (eine Spalte = vertikal gestapelt). Auf
  `wxRA_SPECIFY_ROWS, 1` umgestellt (eine Zeile = alle Optionen
  nebeneinander) – reine Orientierungsänderung, keine Options- oder
  Verhaltensänderung.
- **"Preview"** ist eine eigenständige, umrandete Gruppe
  (`wxStaticBoxSizer`) – vorher stand dort nur eine einfache
  Textbeschriftung ohne eigenen Rahmen. Alle drei Elemente
  (Play/Stop-Button, "Loop", "Back and forth") stehen in einer
  horizontalen Zeile innerhalb dieser Box, statt Button und
  Checkboxen auf zwei getrennten Zeilen. (Zunächst gleichrangig neben
  "Animation" umgesetzt, auf Wunsch direkt danach in "Animation"
  hinein verschachtelt – siehe Nachtrag unten.)
- **"Lock Palette"** → **"Lock current palette"** umbenannt.
- **"Interlace (LACE)"** → **"Interlace flag"** umbenannt.

**Verifiziert**: Screenshot bestätigt alle fünf Änderungen exakt wie
gewünscht (drei horizontale Radiogruppen, eigene "Preview"-Box mit
allen drei Elementen nebeneinander, beide neuen Beschriftungen). Da
das Umhängen der Preview-Widgets auf eine neue `wxStaticBox` als
Elternfenster den Freischalt-Mechanismus (`Enable()`/`Disable()` in
`UpdateKeyframeControlsEnabled()`) hätte betreffen können, zusätzlich
per Debug-Hotkey bestätigt: nach Aktivieren von Crop und Animation
sind Play-Button, "Loop" und "Back and forth" korrekt freigeschaltet
(`play=1 loop=1 pingpong=1`), passend auch per Screenshot bestätigt
(alle drei nicht mehr ausgegraut). Präprozessor-Direktiven nach
Debug-Code-Entfernung auf Balance geprüft (0 ifdef / 0 endif),
CLI-Baseline unverändert (`0,0126422`), Release-Build sauber (0
Warnungen), breiter GUI-Sanity-Lauf ohne neue Warnungen.

**Nachtrag – "Preview" auf Wunsch doch in "Animation" verschachtelt**:
Die "Preview"-Box wird jetzt als Kind von "Animation"s eigenem Sizer
eingehängt (`box->Add(preview_box, …)` statt `parent_sizer->Add(…)`),
mit `box->GetStaticBox()` statt `this` als Elternfenster für die
verschachtelte `wxStaticBox` – der korrekte wx-Aufbau für ineinander
liegende Gruppenboxen. Rahmen, Titel und horizontale Anordnung der
drei Elemente bleiben unverändert, nur die Position wechselt von
gleichrangig neben "Animation" zu einer Untergruppe darin. Erneut per
Debug-Hotkey und Screenshot bestätigt: die verschachtelte Box schaltet
sich weiterhin korrekt frei, sobald Crop und Animation aktiviert sind
(`play=1 loop=1 pingpong=1`). CLI-Baseline unverändert, Release-Build
sauber (0 Warnungen), Präprozessor-Direktiven auf Balance geprüft (0
ifdef / 0 endif).

## Farbkorrektur: nur im Ausgabebild sichtbar

Vier optionale Regler (`--brightness`, `--contrast`, `--gamma`,
`--saturation`, alle Standardwert = unverändert) werden auf die bereits
zugeschnittene und skalierte Leinwand angewendet – **direkt bevor**
Quantisierung/Dithering laufen (`ConversionPipeline::preview`, nach
`resize_image`, vor dem Encoder). Ein GUI-Quellbild-Panel, das die
unveränderte Originaldatei zeigt, durchläuft diese Pipeline nie und
bleibt daher automatisch unberührt – "nur im Ausgabebild sichtbar" ergibt
sich aus der Architektur, nicht aus einer eigenen Sonderfall-Prüfung.

Reihenfolge und Formeln (`color_correction.cpp`, jeweils auf die
sRGB-kodierten Werte direkt, wie ein typischer Foto-Editor-Regler –
keine linear-radiometrische Umrechnung):

1. **Helligkeit** (additiv): `output = input + brightness`
2. **Kontrast** (um den Mittelpunkt 0,5): `output = (input − 0,5) × contrast + 0,5`
3. **Gamma** (Potenzkurve): `output = input^(1/gamma)`
4. **Sättigung** (Mischung mit Rec.-709-Luma-Grau): `output = grau + (input − grau) × saturation`

Nach jedem Schritt wird auf [0,1] geklemmt. Bei allen vier Werten auf
Standard (`is_identity()`) ist die Funktion ein sofortiger No-op – keine
Allokation, keine Bildschleife.

**Verifiziert**: sieben eigenständige, exakte Zahlentests (kein GUI, kein
Xvfb nötig) – z. B. Helligkeit 0,3+0,2 ergibt exakt 0,5; Kontrast
(0,75−0,5)×2+0,5 ergibt exakt 1,0; Gamma 0,25^(1/2) ergibt exakt 0,5;
Sättigung 0 auf reines Rot ergibt exakt (0,2126, 0,2126, 0,2126); alle
Standardwerte lassen ein Testpixel bit-exakt unverändert. Zusätzlich:
eine komplette Konvertierung mit expliziten Identity-Flags
(`--brightness 0 --contrast 1 --gamma 1 --saturation 1`) ist **bytegleich**
mit derselben Konvertierung ganz ohne diese Flags. Eine farbkorrigierte
Beispielausgabe besteht die `netpbm`-Validierung.

**GUI-Nachtrag**: eigene Box "Target color correction" (ursprünglich
"Color correction (output only)" genannt, dann umbenannt) mit vier
Schiebereglern (Helligkeit, Kontrast, Gamma, Sättigung, ganzzahlige
Slider-Werte auf denselben Float-Bereich abgebildet wie
`chroma_weight_`/`pixel_aspect_`) in einem 2×2-Raster (Helligkeit
oben links, Kontrast oben rechts, Gamma unten links, Sättigung unten
rechts) statt vier Reglern untereinander. Zwischenzeitlich in die
linke Spalte verschoben, auf Wunsch wieder zurück in die **rechte**
Spalte, jetzt direkt **oberhalb** von "HAM conversion settings"
(ursprünglich "Color processing" genannt) – bewusst weiterhin eine
eigene Box: dort stehen Algorithmus-*Wahlen* (welcher Ditherer,
welcher Quantizer), hier Ton-*Anpassungen* auf der Leinwand, das
zusammen in eine Box zu packen wirkte beim Bauen eher verwirrend als
hilfreich. In "Save current Settings"/`LoadDefaults` eingebunden wie
die anderen Regler. Refinement- und Chroma-Regler stehen inzwischen
ebenfalls nebeneinander statt untereinander (dasselbe wachsende
`wxFlexGridSizer`-Muster wie beim 2×2-Raster).

Zusätzlich ein **"Reset"-Button** unterhalb des Rasters, rechtsbündig:
setzt alle vier Regler in einem Aufwasch auf ihre Standardwerte zurück
(Helligkeit 0, Kontrast/Gamma/Sättigung je 1,0) und aktualisiert dabei
auch die vier Beschriftungen – `wxSlider::SetValue()` löst kein
Änderungs-Ereignis aus, daher geschieht das explizit im selben
Handler, genau wie an den anderen Stellen in dieser Datei, die einen
Regler programmatisch setzen (z. B. `LoadDefaults`).

**Verifiziert** per Debug-Log und Screenshot-Differenz (Xvfb + fluxbox +
xdotool): Regler-Wert fließt korrekt von der GUI in die Optionen
(`brightness=0.20` nach 20 Pfeiltasten-Schritten auf einem zuvor bei 0
stehenden Regler). Eine Konvertierung mit Helligkeit 0,40 + Kontrast
1,60 ändert den gemeldeten Fehler messbar (`0,012642 → 0,010262`) –
kein stiller No-op. Endgültige Platzierung direkt per
Bildschirmposition bestätigt: "Target color correction" und "HAM
conversion settings" auf identischer X-Koordinate (beide rechte
Spalte), Farbkorrektur-Box mit kleinerer Y-Koordinate (= oberhalb).
Reset-Button: alle vier Regler zunächst auf unterschiedliche
Nicht-Standardwerte gestellt (`b=10 c=115 g=125 s=70`), nach Klick auf
"Reset" korrekt und vollständig zurückgesetzt (`b=0 c=100 g=100
s=100`), Beschriftungen entsprechend aktualisiert. **Wichtigster
Test**: Screenshot vor und nach einer extremen Reglerverstellung
pixelgenau auf den exakten
Quellbild-Panel-Bereich zugeschnitten und verglichen – Bounding-Box
aller Unterschiede `None`, maximale Differenz `0`, Summe aller
Differenzen `0` – das Quellbild bleibt tatsächlich vollständig
unberührt, nicht nur der Architektur nach angenommen. "Save current
Settings" mit Helligkeit=0,15 geklickt, Konfigurationsdatei zeigt
korrekt `Brightness=15`; frischer Programmstart lädt `brightness=0.15`
korrekt zurück.

## Palette-Optimierung: Verfeinerung + Farbfokus

`MedianCutQuantizer` liefert nur eine Saat-Palette aus der reinen
Farbverteilung – unabhängig davon, ob Dithering aktiv ist, ob die
16 Farben gut als HAM6-Anker taugen, oder ob manche Bildregionen
wichtiger sind als andere. Zwei zusätzliche, kombinierbare Bausteine:

**Iterative Verfeinerung** (`HAM6Encoder`, Standard: 4 Durchläufe): nach
der Saat-Palette läuft der *echte* Encoder inklusive des *echten*
gewählten Ditherers probeweise mehrfach durch – welche Pixel wählten
welchen Paletteneintrag per Select-Mode (Mode 00)? Die Palette wird
danach als gewichteter Mittelwert der jeweils zugeordneten Pixel neu
berechnet (K-Means-artig). Weil die Testdurchläufe den echten Diffuser
nutzen (inkl. Reset über `begin()`), ist das automatisch
Dithering-bewusst – keine Sonderfall-Logik nötig. Gemessene Wirkung auf
dem generischen Testbild: mittlerer OKLab-Fehler **0,01509 → 0,01260
(≈16,5 % weniger)**, bei weiterhin Sub-Sekunden-Laufzeit. Mit
`--palette-iterations 0` reproduziert es exakt den alten Wert.

**Farbfokus** (GUI-Label "Color focus", CLI `--color-focus`; intern
weiterhin `IImportanceMap` – der Name wurde bewusst nur an der
Nutzeroberfläche geändert, nicht im internen Code, um das Risiko eines
größeren, rein kosmetischen Refactorings zu vermeiden): eine
eigenständige Pixel-Gewichtsfunktion, die sowohl in den gewichteten
Median-Cut (Saat) als auch in die gewichtete Mittelwertbildung der
Verfeinerung einfließt. `center` gewichtet nach Bildmitte (Gaußartiger
Abfall, Bodenwert damit der Rand nicht komplett ignoriert wird), `edge`
nach lokalem Farbgradient in OKLab (zielt auf Fringing: mehr Anker dort,
wo sonst teure Mehrkanal-Sprünge nötig wären), `center-edge`
kombiniert beides multiplikativ, `golden-ratio` gewichtet nach der
klassischen Kompositionsregel: statt einem einzelnen Zentrum vier
Punkte bei 1/φ und 1/φ² (~61,8 % / ~38,2 %) auf jeder Achse – dieselbe
Gauß-Abfall-Form wie `center`, nur um vier "Bildregeln"-Punkte statt
um einen. Direkt geprüft (rein geometrisch, ohne Bildinhalt): alle vier
Golden-Punkte erreichen Gewicht ≈1,0, die exakte Bildmitte nur ≈0,86
(spürbar niedriger – die Regel bevorzugt bewusst NICHT die Mitte),
Ecken ≈0,265 (nahe am Bodenwert 0,15).

**Ehrlicher Befund zur Wirkung von `center`:** auf einem gezielten
Stresstest-Bild (`testdata/center_stress.png` – unruhiger, farbenfroher
Rand vs. dezentes, farblich enges Motiv in der Mitte) sinkt der Fehler
im Zentrum von 0,02447 (uniform) auf 0,02386 (center) – ein realer,
aber bescheidener Effekt (~2,5 %). Grund: die Cluster-*Zuordnung*
bleibt reine nächste-Farbe-Distanz; die Gewichtung wirkt nur auf den
Mittelwert bereits gebildeter Cluster, nicht auf deren Anzahl. Ein
Versuch, das zu verstärken (unbedingtes Umverteilen des am wenigsten
genutzten Paletteneintrags an das am schlechtesten getroffene Pixel
pro Durchlauf) wurde implementiert, gemessen, und **wieder verworfen**:
es destabilisierte die Konvergenz und war beim Stresstest sogar
schlechter als uniform. Reproduzierbar mit `testdata/analyze_importance.py`.
Ein sauberer nächster Schritt wäre eine budget-bewusste Neuzuteilung
(z. B. Hill-Climbing: Tausch nur behalten, wenn er den gewichteten
Gesamtfehler nachweislich senkt) statt der jetzigen ungebremsten Variante.

## Weitere Dithering- und Quantisierungsverfahren

**Dithering** – die Auswahl folgt der Fehlerausbreitungs-Achse aus der
Fringing-Diskussion (wie viel Fehler wird gestreut, wie weit):

- `atkinson` – Bill Atkinsons Verfahren (original Macintosh). Streut nur
  6/8 = 75 % des Restfehlers, der Rest wird verworfen – dadurch höherer
  Kontrast, "sauberer" als volle Fehlerdiffusion.
- `sierra-lite` – schlanke 3-Ziele-Alternative zu Floyd-Steinberg, volle
  Fehlerausbreitung, aber schmalere Streuung.
- `jarvis-judice-ninke` – breiter 12-Ziele-Kernel über 2 Zeilen, glattere
  Verläufe, aber die größte Streuweite aller hier verfügbaren Verfahren.
- `blue-noise` – Ordered Dithering wie `bayer`, aber mit Interleaved-
  Gradient-Noise (Jimenez 2014) statt fester Bayer-Matrix als
  Schwellwert-Quelle – kein sichtbares, mechanisches Rastermuster, keine
  Vorberechnungstabelle nötig.

Alle drei neuen Fehlerdiffusions-Verfahren nutzen eine gemeinsame,
generische `KernelErrorDiffuser`-Basisklasse (`aic/dithering/kernel_diffuser.hpp`) –
jedes Verfahren ist nur eine Liste von `(dx, dy, Gewicht)`-Zielen plus
Name, keine eigene Pufferverwaltung nötig.

**Gemessen** (Farbrauschen im Graustufen-Streifen des Testbilds, ein
direktes Fringing-Maß – höher = mehr farbiges Rauschen in einem
eigentlich neutralen Bereich):

| Ditherer | Chroma-Rauschen |
|---|---|
| `bayer` | 0,00310 |
| `blue-noise` | 0,00325 |
| `atkinson` | 0,00415 |
| `sierra-lite` | 0,00424 |
| `floyd-steinberg` | 0,00463 |
| `jarvis-judice-ninke` | 0,00482 |

Exakt die vorhergesagte Reihenfolge: Ordered-Verfahren ohne
Fehlerausbreitung fringen am wenigsten, `jarvis-judice-ninke`s breiter
Streubereich am meisten, `atkinson`s reduzierte Fehlermenge liegt
spürbar unter `floyd-steinberg`.

**Quantisierung:**

- `kmeans` – klassisches K-Means (Lloyd-Algorithmus) in OKLab-Raum, als
  strukturell anderer Ansatz zu Median-Cut. Deterministisch: die
  Startzentren kommen aus gewichteter Greedy-Farthest-Point-Auswahl
  (kein Zufall, reproduzierbare Ergebnisse) statt zufälliger
  Neustarts wie beim klassischen k-means++.
- `octree` – klassische Octree-Quantisierung (Gervautz & Purgathofer,
  1988). Arbeitet bewusst direkt auf den 8-Bit-sRGB-Bytes statt in
  OKLab – das ist keine Inkonsistenz, sondern eine Eigenschaft des
  Algorithmus selbst (die Baumstruktur nutzt die Bit-Repräsentation der
  Kanalwerte). Nur der letzte Schritt (Blattfarbe → nächste Amiga-Farbe)
  läuft wie überall sonst über die OKLab-Suche.

Bewusst **nicht** hinzugefügt: reines Zufallsrauschen-Dithering (strikt
schlechter als `blue-noise`), Stucki (zu ähnlich zu JJN, kein echter
Mehrwert), Popularity/Histogramm-Quantisierung (für Fotos strukturell
Median-Cut/Octree unterlegen).



Die gesamte Fehlerberechnung läuft in **OKLab** (`aic/color_space.hpp`),
euklidische Distanz zwischen Ziel- und erreichbarer Farbe. Das betraf
bisher mit einer Ausnahme alles: die HAM6-Kandidatensuche, die
Cluster-Zuordnung der Verfeinerung, `EdgeWeightedImportance`, die
`mean_oklab_error`-Metrik.

**Behobene Inkonsistenz:** `quantize_to_amiga()` (Umwandlung einer
berechneten Ziel-OKLab-Farbe – Median-Cut-Box-Mittelwert,
Verfeinerungs-Cluster-Mittelwert – in einen speicherbaren 4-Bit-Wert)
rundete bisher jeden RGB-Kanal unabhängig in sRGB-Raum. Das minimiert
nicht zuverlässig die OKLab-Distanz, weil OKLab eine nichtlineare
Transformation ist. Ersetzt durch `quantize_oklab_to_amiga()` – echte
Nearest-Neighbor-Suche über alle 4096 darstellbaren Amiga-Farben, mit
derselben LUT, die der Encoder ohnehin für die Pixel-Kandidatensuche
aufbaut (jetzt geteilt statt dupliziert). Isoliert gemessen (Saat-Palette
allein, ohne Verfeinerung): mittlerer OKLab-Fehler **0,01509 → 0,01496**
– ein garantierter, kleiner Gewinn. Mit aktivierter Verfeinerung
verschiebt sich der Endwert leicht (0,01260 → 0,01264): der iterative
Prozess konvergiert wegen der pro Schritt jetzt anderen (korrekteren)
Rundung zu einem *anderen* lokalen Optimum – nicht zwingend schlechter,
nur ein anderer Fixpunkt. Ehrlich erwähnt, weil die Zahl sonst
überraschend in die falsche Richtung zu laufen scheint.

**Konfigurierbare Wahrnehmungsgewichtung** (`--chroma-weight`,
`PerceptualWeights` in `color_space.hpp`): das menschliche Auge löst
Helligkeitsunterschiede deutlich feiner auf als Farbtonunterschiede bei
gleicher räumlicher Frequenz – die Begründung hinter Chroma-Subsampling
in JPEG/Video. `weighted_distance2()` gewichtet den a/b-Anteil der
OKLab-Distanz mit einem Faktor `chroma_weight` (Default 1,0 =
unverändertes Verhalten); Werte <1 lassen den Encoder bei
Kandidaten-Kompromissen eher Helligkeit exakt treffen und bei der
Farbe nachgeben. Fließt in Kandidatensuche, Worst-Pixel-Verfolgung und
Palettenanpassung ein – die extern berichtete `mean_oklab_error`-Metrik
bleibt bewusst ungewichtet, damit Ergebnisse über verschiedene
`--chroma-weight`-Werte hinweg vergleichbar bleiben (sonst würde jede
Gewichtung sich selbst automatisch besser bewerten).

Gemessen (separater L- und Chroma-Fehler, nicht die kombinierte
Metrik): mit sinkendem `chroma-weight` sinkt der mittlere
Helligkeitsfehler monoton (0,21003 → 0,20908 bei 1,0 → 0,1), der
Farbfehler steigt monoton (0,05480 → 0,05922) – der beabsichtigte
Trade-off funktioniert wie vorgesehen.

**Nicht umgesetzt** (siehe Diskussion): eine räumlich/frequenzbewusste
Metrik (S-CIELAB-artig, mit Vorfilterung vor dem Pixel-Vergleich) oder
eine CRT-Aperture-Simulation vor dem Fehlervergleich – beides würde
echten Zusatzaufwand bedeuten (Faltung/Filterpass) statt der aktuellen
rein punktweisen Berechnung.

## Fünf weitere GUI-Anpassungen: Settings-Position, Save-As-Standardverzeichnis, Convert/Play-Preview-Verschmelzung, Groß-/Kleinschreibung, About-Dialog

Auf konkreten Wunsch fünf unterschiedliche Änderungen:

**1. "Settings..." nach unten links verschoben** – aus dem oberen
Toolbar entfernt, jetzt direkt neben "Save current settings" im
unteren Bereich.

**2. "Save As..." nutzt das Zielverzeichnis als Standard** – ist
bereits eines über "Target directory..." gewählt, öffnet sich der
native Speichern-Dialog direkt dort statt im zuletzt vom
Betriebssystem gemerkten Verzeichnis (`wxFileDialog`s dritter
Konstruktor-Parameter, vorher leer, jetzt `target_dir_`).

**3. "Convert"-Button eliminiert, in "Play Preview" verschmolzen**:
Bei deaktivierter Animation heißt der Button jetzt **"Show HAM
preview"**, ist immer aktiv (nicht mehr an eine Keyframe-Anzahl
gebunden) und löst dieselbe Einzelbild-Konvertierung aus, die vorher
der eigenständige "Convert (F5)"-Button übernahm – inklusive F5 als
Tastenkürzel, jetzt umgeleitet auf `OnPlayPreviewToggled()`. Bei
aktivierter Animation bleibt das bisherige Verhalten unverändert
("Play preview"/"Stop preview", nur bei ≥2 Keyframes aktiv).
`ParameterPanel::UpdatePlayPreviewButtonLabel()` entscheidet zentral
zwischen beiden Beschriftungen; sowohl `UpdateKeyframeControlsEnabled()`
als auch `SetPreviewPlaying()` rufen sie auf, damit das Label in
jedem Fall korrekt bleibt, unabhängig davon, welcher der beiden Wege
zuletzt den Zustand geändert hat.

**4. Durchsicht der Groß-/Kleinschreibung**: die meisten mehrwortigen
Labels folgten bereits "Sentence case" (nur das erste Wort groß, z. B.
"Enable crop", "Lock current palette"), einige neuere wichen davon ab
und wurden angeglichen: "Enable Animation"→"Enable animation", "Add
Keyframe"→"Add keyframe", "Load/Save Palette..."→"Load/save
palette...", "Single Images"→"Single images" (jetzt konsistent mit
"Back and forth" in derselben Radiogruppe), "Play/Stop
Preview"→"Play/Stop preview" (jetzt auch konsistent mit "Show HAM
preview", derselbe Button), "Save current Settings"→"Save current
settings" (vorher influenzierte Groß-/Kleinschreibung innerhalb eines
einzelnen Labels). Bewusst **nicht** angeglichen: "Save As..." und das
dazu parallele "Export Animation As..." – "Save As" ist eine so
etablierte, universell wiedererkennbare UI-Konvention (durchgehend
Title Case), dass eine Umstellung auf Sentence Case eher befremdlich
gewirkt hätte; da beide Buttons denselben "Export Animation"-Anteil
teilen, wurde konsequent auch "Export Animation" (ohne "As") auf
Title Case belassen statt intern inkonsistent zu werden. Zusätzlich
zwei **sichtbare** Dialogmeldungen gefunden und korrigiert, die noch
den alten Button-Namen "Add Keyframe" zitierten (wären nach der
Umbenennung nicht mehr zum tatsächlichen Button-Text passend
gewesen), sowie mehrere rein interne Code-Kommentare aktualisiert.

**5. Neuer About-Dialog** (`AboutDialog.hpp/cpp`, nach dem Vorbild von
`SettingsDialog`), geöffnet über einen neuen "About"-Button unten
rechts – exakt an der Position, an der vorher "Convert" stand. Nennt
"rawbit" und Claude (Anthropic) als Ersteller.

**Verifiziert** (Debug-Hotkeys für die interaktiven Teile, da einzelne
Klicks in dieser Sandbox wiederholt unzuverlässig registrierten –
unter anderem wurde dabei einmal mehr sichtbar, dass ein Klick auf
eine falsche Stelle traf, weil ein Config-Zustand aus einer früheren
Testsitzung nicht bereinigt worden war, nicht weil der Button selbst
fehlerhaft reagierte):
- Screenshot bestätigt das komplette neue Layout: "Settings..." unten
  links neben "Save current settings", "About" unten rechts an
  Convert's alter Stelle, "Show HAM preview" korrekt angezeigt (da
  Animation zu Beginn deaktiviert ist)
- F5 mit deaktivierter Animation ausgelöst: echte Konvertierung lief
  durch, Statuszeile zeigt korrekt "320x256 error=0.0126 LACE=off",
  Amiga-Vorschau und "Current palette" zeigen das tatsächliche
  Ergebnis – exakt das frühere Convert-Verhalten
- About-Dialog per Debug-Hotkey geöffnet und der Inhalt per Screenshot
  bestätigt: Titel "Amiga Image Converter", Beschreibung, sowie
  "Created by rawbit, together with Claude (Anthropic)."
- Umschalten von Animation zweimal per Debug-Hotkey getestet: aktiv →
  `label='Play preview' enabled=0` (0 Keyframes vorhanden), inaktiv →
  `label='Show HAM preview' enabled=1` – exakt wie spezifiziert
- Präprozessor-Direktiven nach Debug-Code-Entfernung auf Balance
  geprüft (0 ifdef / 0 endif), CLI-Baseline unverändert (`0,0126422`),
  Release-Build sauber (0 Warnungen), breiter GUI-Sanity-Lauf ohne
  neue Warnungen

## Durchdachte Tastenkürzel für die wichtigsten Funktionen

Auf Vorschlag ("könnte möglicherweise auch deinen Testaufwand
vereinfachen") ein bewusst kuratiertes, kleines Set an
Tastenkürzeln für die wichtigsten Funktionen ergänzt – nicht jede
denkbare Aktion, sondern gezielt die mit dem größten Nutzen:

- **Ctrl+O** – Bild öffnen (universelle Konvention)
- **Ctrl+S** – Speichern im Zielverzeichnis (matcht exakt die
  bestehende Bedeutung des "Save"-Buttons: "speichere am zuletzt
  benutzten Ort")
- **Ctrl+Shift+S** – Speichern mit Dateiauswahl
- **F5** – Convert/Play-Preview (bereits bestehend, unverändert;
  löst je nach Animationsstatus Einzelbild-Konvertierung oder
  Animationswiedergabe aus, siehe vorherige Änderungsrunde)
- **Ctrl+K** – Add keyframe (die während des Animation-Aufbaus am
  häufigsten wiederholte Einzelaktion – K für "Keyframe")
- **Ctrl+Q** – Beenden (universelle Konvention, auch wenn der
  native Fenster-Schließen-Button/Alt+F4 das bereits abdecken)

Bewusst **nicht** mit Tastenkürzeln belegt: bloße Buchstaben ohne
Ctrl/Alt-Modifikator (Konfliktrisiko mit normaler Texteingabe in
Zahlenfeldern, falls diese den Fokus haben) sowie Delete/Backspace
für "Remove Keyframe" (aus demselben Grund – diese Tasten werden in
Zahlenfeldern für normales Editieren gebraucht). "Enable crop"/"Enable
animation" bewusst ohne eigenes Kürzel gelassen, da diese typischerweise
nur einmal pro Sitzung umgeschaltet werden, nicht wiederholt.

Jedes neue Kürzel ist über einen Tooltip am zugehörigen Button
auffindbar (z. B. "Open..." zeigt "Open an image (Ctrl+O)"); der
Play/Stop-Vorschau-Button aktualisiert seinen Tooltip zusätzlich
dynamisch je nach aktuellem Modus. Der About-Dialog listet zusätzlich
alle acht Kürzel (inklusive der bereits bestehenden Zoom-Kürzel) in
einer eigenen "Keyboard shortcuts"-Box übersichtlich auf.

Technisch: `open_btn`/`save_quick_btn_`/`save_as_btn_` hatten bereits
eigene `wxEVT_BUTTON`-Bindungen; die neuen Kürzel binden zusätzlich
`wxEVT_MENU` auf dieselben Handler (exakt das Muster, das F5/Convert
schon vorher nutzte) – kein Risiko einer doppelten Bindung, da beide
Wege unterschiedliche Event-Typen erzeugen. Für "Add keyframe" (dessen
Erfassungslogik komplett privat in `ParameterPanel` liegt) ein
schlanker öffentlicher Wrapper `TriggerAddKeyframe()` ergänzt, der 1:1
dasselbe tut wie ein Klick auf den Button – inklusive dessen sicherem
Nichtstun, wenn Zuschnitt nicht aktiviert ist.

**Verifiziert** (mit echten Tastenkombinationen über `xdotool key`,
genau wie ein Nutzer sie eingeben würde – bestätigt den Vorschlag, dass
dies den Testaufwand tatsächlich vereinfacht, da einzelne Klicks in
dieser Sandbox wiederholt unzuverlässig registrierten, während
Tastatureingaben durchgehend zuverlässig ankamen):
- **Ctrl+O**: nativer "Open Image"-Dialog öffnet sich korrekt
  (Screenshot)
- **Ctrl+S**: Datei tatsächlich im konfigurierten Zielverzeichnis
  erzeugt (auf der Platte bestätigt)
- **Ctrl+Shift+S**: "Save IFF As"-Dialog öffnet sich und startet dabei
  korrekt bereits im gesetzten Zielverzeichnis – bestätigt gleichzeitig
  auch die entsprechende Funktion aus der vorherigen Änderungsrunde
  (Screenshot)
- **Ctrl+K**: als sicherer No-Op ohne aktivierten Zuschnitt getestet
  (kein Absturz); mit aktiviertem Zuschnitt und aktivierter Animation
  dreimal hintereinander gedrückt – Keyframe-Zähler steigt korrekt
  1 → 2 → 3
- **Ctrl+Q**: Prozess lief vor dem Tastendruck nachweislich, war
  danach nachweislich beendet
- Der neue "Keyboard shortcuts"-Bereich im About-Dialog rendert sauber
  und vollständig (Screenshot, acht Einträge mit fett hervorgehobener
  Taste und Beschreibung)
- Präprozessor-Direktiven nach Debug-Code-Entfernung auf Balance
  geprüft (0 ifdef / 0 endif), CLI-Baseline unverändert (`0,0126422`),
  Release-Build sauber (0 Warnungen), breiter GUI-Sanity-Lauf ohne
  neue Warnungen

## "Add Keyframe" wählt den neuen Eintrag wieder automatisch aus (mit verifiziertem Kompromiss)

Auf ausdrücklichen Wunsch die Auto-Auswahl nach "Add keyframe" wieder
eingebaut: der neu erzeugte Keyframe wird jetzt in der Combobox
ausgewählt, und alle Bedienelemente (Zahlenfelder, Rubber-Band)
beziehen sich sofort auf ihn – exakt wie bei jedem anderen, bereits
bestehenden, ausgewählten Keyframe.

Das ist dieselbe Änderung, die schon einmal eingebaut, dann wegen
eines gefundenen Fehlers zurückgebaut wurde (siehe weiter oben). Auf
ausdrückliche Bitte diesmal **gezielt auf genau diesen möglichen
Seiteneffekt geprüft**, mit dem Ergebnis:

- **Sicherer Ablauf**: "Add keyframe" → Position ändern (aktualisiert
  diesen, noch ausgewählten Eintrag) → "Add keyframe" erneut (zweigt
  vom jetzt veränderten Stand einen neuen, wieder ausgewählten Eintrag
  ab) → Position ändern → … Verifiziert: Add (100) → Breite 150 → Add
  → Breite 200 ergibt korrekt Keyframe 1 = 150, Keyframe 2 = 200 –
  beide bleiben unabhängig und unterschiedlich.
- **Unsicherer Ablauf, reproduziert den früheren Fehler**: Position
  ändern direkt nach "Add keyframe", *bevor* das nächste Mal "Add
  keyframe" geklickt wird. Verifiziert: Add (100) → Breite 150 → Add
  erneut ergibt **beide** Keyframes bei 150 – der ursprüngliche Wert
  100 des ersten Keyframes ist verloren, exakt derselbe Fehler wie
  zuvor.

Der Unterschied zur früheren Fassung liegt also nicht darin, dass der
Fehler behoben wurde, sondern darin, dass er jetzt **bewusst in Kauf
genommen** wird, weil die Auto-Auswahl explizit gewünscht war. Der
korrekte Arbeitsablauf für eine Sequenz mehrerer, unterschiedlicher
Keyframes: **zuerst "Add keyframe" klicken, dann erst die Position
ändern** – nicht umgekehrt.

**Verifiziert** (Debug-Hotkeys plus dem echten Ctrl+K-Tastenkürzel für
den "Add keyframe"-Schritt selbst, da einzelne Klicks in dieser
Sandbox wiederholt unzuverlässig registrierten): beide oben
beschriebenen Abläufe exakt wie beschrieben reproduziert, mit
identischen Ergebnissen bei wiederholten Testläufen.
Präprozessor-Direktiven nach Debug-Code-Entfernung auf Balance geprüft
(0 ifdef / 0 endif), CLI-Baseline unverändert (`0,0126422`),
Release-Build sauber (0 Warnungen), breiter GUI-Sanity-Lauf ohne neue
Warnungen.

## Umbenennung: "Amiga Image Converter" → "renderHAM"

Auf Wunsch nach Namensrechts-Prüfung ("HAMify" kollidierte mit einem
bereits existierenden, gleichnamigen Amiga-HAM-Konverter, "renderHAM"
zeigte bei derselben Prüfung keine Kollision) auf die neue
Produktfamilie umgestellt:

- CLI-Tool: `aic_cli` → **`renderHAM`**
- GUI-Tool: `aic_gui` → **`renderHAMgui`**
- Bibliothek: `aic` → **`renderhamlib`** (auch dieser Name vorab
  geprüft – "hamcore" und "hamkit" waren beide bereits anderweitig
  belegt)

Bewusst in zwei Kategorien unterteilt:

- **Umgesetzt** (nutzersichtbare Ebene): Executable-Namen,
  CMake-Projekt-/Zielnamen, alle drei Verzeichnisse (`renderhamlib/`,
  `tools/renderham/`, `tools/renderhamgui/`) inklusive des
  Top-Level-Projektverzeichnisses selbst, Fenstertitel, About-Dialog,
  `wxConfig`-App-Name, Übersetzungskatalog (Dateiname, Paketname,
  `AddCatalog`-Lookup), README-Titel und alle Pfad-/Programmnamen
  darin.
- **Bewusst unverändert gelassen** (interne Ebene, siehe Diskussion
  weiter oben zu dieser Entscheidung): der C++-Namensraum `aic::`
  sowie die Include-Konvention `#include "aic/....hpp"` – beide
  ziehen sich durch praktisch die gesamte Bibliothek, eine
  vollständige Umbenennung wäre ein deutlich größerer, riskanterer
  Eingriff ohne klaren Zusatznutzen gegenüber der nach außen sichtbaren
  Umbenennung.
- Die Frage der Konfigurationsdatei-Migration (der `wxConfig`-Name
  bestimmt den Speicherort gespeicherter Einstellungen) wurde
  ausdrücklich nicht behandelt, da es noch kein echtes Release gibt.

**Verifiziert:**
- Sauberer Build von Grund auf (`rm -rf build`) mit den neuen Namen,
  0 Warnungen, CLI-Baseline unverändert (`0,0126422`)
- `librenderhamlib.a` als tatsächlicher Build-Output bestätigt
  (entspricht der README-Beschreibung)
- Fenstertitel per Screenshot bestätigt: "renderHAMgui" (auch in der
  Locale-Warnmeldung selbst korrekt übernommen)
- About-Dialog per Screenshot bestätigt: Titel "renderHAMgui",
  Tastenkürzel-Liste und Credits unverändert korrekt
- Deutsche Übersetzung vollständig getestet: `renderhamgui.mo` wird
  korrekt erzeugt und geladen, alle Labels übersetzt, Fenstertitel
  bleibt richtigerweise unübersetzt (Eigenname)
- Übersetzungskatalog vollständig: 141 von 141 Strings übersetzt,
  keine fehlenden oder veralteten Einträge, Header-Metadaten
  (Paketname, Copyright-Zeile) aktualisiert
- README vollständig durchsucht: alle Pfad- und Programmnamen-Referenzen
  konsistent aktualisiert (inklusive zweier zunächst übersehener
  Stellen: `libaic.a` als Dateiname sowie ein Beispielaufruf von
  `aic_link_whole_archive`), verbleibende `aic::`/`aic/*.hpp`-Erwähnungen
  gezielt geprüft und als korrekt unverändert bestätigt (Kategorie B)
- Breiter GUI-Sanity-Lauf ohne neue Warnungen

## Debian-Paket bauen (`scripts/build_deb.sh`)

Baut ein installierbares `.deb`-Paket auf Basis der bestehenden
`cmake --install`-Regeln – ersetzt das bisherige rohe
"`sudo cmake --install build`" durch ein Paket, das sich mit
`apt install`/`apt remove` sauber verwalten (und vor allem wieder
vollständig entfernen) lässt.

```bash
./scripts/build_deb.sh            # Version aus Git-Commit-Anzahl (0.<N>.0)
./scripts/build_deb.sh 1.2.0      # Version explizit vorgeben
```

Ergebnis liegt danach in `dist/renderham_<Version>_<Arch>.deb`.

**Was das Skript automatisch erledigt:**
- Konfiguriert + baut das Projekt (Release), installiert per `DESTDIR`
  in ein temporäres Staging-Verzeichnis (Präfix `/usr`)
- Ermittelt die Laufzeit-Abhängigkeiten automatisch über
  `dpkg-shlibdeps` (liest die tatsächlichen Shared-Library-Versionen
  von diesem System, statt Paketnamen zu erraten – wichtig, da sich
  z. B. `libwxgtk3.2-1` vs. `libwxgtk3.2-1t64` je nach
  Ubuntu-/Debian-Version unterscheidet)
- Erzeugt eine vollständige Man-Page für `renderHAM` automatisch über
  `help2man` (liest dessen `--help`-Ausgabe aus)
- Schreibt eine einfache, von Hand verfasste Man-Page für
  `renderHAMgui` – `help2man` funktioniert hier nicht automatisch, da
  wxWidgets für `--help` bereits GTK+ initialisiert und dafür ein
  laufendes Display braucht
- Entfernt Debug-Symbole aus beiden Binaries (`strip`)
- Schreibt `DEBIAN/control`, `copyright` und ein minimales Changelog
  (natives Debian-Paket)

**Bewusste Design-Entscheidung – Übersetzungsverzeichnis bleibt
nicht-FHS-konform:** `renderHAMgui` sucht seine Übersetzung zur
Laufzeit relativ zum eigenen Programmverzeichnis
(`<exe-Pfad>/translations/<sprache>/LC_MESSAGES/renderhamgui.mo`,
siehe `Localization.cpp`), nicht am FHS-üblichen Ort
`/usr/share/locale/`. Das Skript übernimmt deshalb bewusst die
bestehende Installationsstruktur unverändert, auch wenn `lintian`
dafür `subdir-in-usr-bin` meldet – eine "Korrektur" hin zum
FHS-Standardpfad würde die Übersetzung nach der Installation still
brechen (fällt dann klaglos auf Englisch zurück, aber Deutsch wäre
weg).

**Verifiziert – vollständiger Lebenszyklus, nicht nur der Build:**
- Paket erfolgreich gebaut, `dpkg-deb --info`/`--contents` zeigen
  korrekte Metadaten und Dateiliste
- `lintian`-Lauf zeigt nach Beheben der leicht behebbaren Punkte
  (Stripping, Changelog, Man-Pages, leeres Homepage-Feld entfernt) nur
  noch die bewusst in Kauf genommene `subdir-in-usr-bin`-Meldung
- Automatisch ermittelte Depends-Zeile bestätigt korrekt für dieses
  System: `libc6, libgcc-s1, libstdc++6, libwxbase3.2-1t64,
  libwxgtk3.2-1t64`
- Tatsächlich per `apt install ./dist/renderham_*.deb` installiert
  (nicht nur gebaut) – danach `dpkg -l renderham` zeigt korrekt
  installiert, beide Binaries über `$PATH` aufrufbar
- CLI-Baseline mit dem **installierten** Binary erneut bestätigt
  (`0,0126422`, identisch zum Entwicklungs-Build)
- GUI mit dem **installierten** Binary unter Xvfb getestet: deutsche
  Übersetzung vollständig aktiv (auch der native "Bild öffnen"-Dialog
  korrekt übersetzt), Fenstertitel korrekt "renderHAMgui"
- `apt remove renderham` vollständig und sauber getestet – Binaries
  und Übersetzungsverzeichnis danach nachweislich verschwunden
- Ein Nebenbefund beim Testen: diese Sandbox ist ein "minimiertes"
  Container-Image mit globalen `dpkg`-Ausschlussregeln
  (`/etc/dpkg/dpkg.cfg.d/`), die bei **jedem** Paket automatisch
  Man-Pages und `/usr/share/locale/*.mo`-Dateien entfernen – betrifft
  nicht `renderhamgui.mo` (liegt bewusst nicht dort, siehe oben) und
  tritt auf einem echten Linux-Mint-Desktop nicht auf

Das (nicht im Repository enthaltene) Skript zum Wiederherstellen der Entwicklungsumgebung wurde um
`dpkg-dev`, `lintian`, `help2man` sowie die passenden Verifikationszeilen ergänzt.

## Ideen aus ham_convert (Block 1: schnelle Qualitätshebel)

Anlass: ein Vergleich mit dem Java-Tool *ham_convert* (Freeware, Quellcode
nicht öffentlich). Umgesetzt werden ausschließlich die in dessen Projektseite
und Changelog **dokumentierten Prinzipien**, unabhängig neu implementiert –
kein Code, kein Dekompilat. Die Reihenfolge ergibt sich aus Nutzen/Aufwand.

### QA-Werkzeuge (Voraussetzung für die folgenden Punkte)

- **`--canvas-png <pfad>`** (CLI) schreibt die Leinwand, die der Encoder
  tatsächlich gesehen hat (nach Zuschnitt, Skalierung, Farbkorrektur) – die
  Referenz, gegen die der gemeldete Fehler gemessen wird.
  `ConversionOptions::canvas_png_path`; `PreviewResult::canvas` wird nur mit
  `ConversionOptions::keep_canvas` befüllt (siehe Punkt 8, Speicherverbrauch).
- **`scripts/ham_eval.py`** vergleicht Einstellungsvarianten objektiv in OKLab:
  `err_px` (mittlerer Pixelfehler), `err_p99` (99. Perzentil – Ausreißer wie
  Fringing), `err_blur` (Fehler nach Gauß-Weichzeichnung σ=1,5 px – das Maß,
  das Dithering belohnt, weil es die Mittelung des Auges nachbildet) sowie
  IFF-Größe und Laufzeit. Beispiel:
  `scripts/ham_eval.py --bin build/tools/renderham/renderHAM --input a.png
  "FS::--dither floyd-steinberg" "Bayer::--dither bayer"`.
- **`scripts/export_sample_photos.py`** schreibt vier frei lizenzierte Fotos
  (scikit-image-Beispieldaten) als PNG für die Auswertung; sie werden nicht
  eingecheckt.

Ausgangslage vor Block 1 (astronaut.png, 320x256, Standardeinstellungen):
Floyd-Steinberg hat mit Abstand den kleinsten `err_blur` (0,0022 gegenüber 0,0071
ohne Dithering), Bayer und Blue-Noise liegen mit 0,0067 kaum besser als gar kein
Dithering – deren Wirkung ist hier also schwach.

### Punkt 3: Dither-Stärke (Propagationsfaktor)

`--dither-strength <0-100>` (CLI, in Prozent) bzw. der Regler "Dither strength"
in der GUI skaliert die Wirkung des gewählten Ditherers. Schnittstelle:
`IErrorDiffuser::set_strength(float)`, `ConversionOptions::dither_strength`
(0..1, Standard 1 = bisheriges Verhalten, Werte außerhalb werden begrenzt).

- **Fehlerdiffusion** (Floyd-Steinberg, Atkinson, Sierra-Lite, Jarvis-Judice-Ninke):
  Anteil des Restfehlers, der an die Nachbarn weitergegeben wird.
- **Ordered** (Bayer, Blue-Noise): Amplitude des Schwellenmusters. (Der Parameter
  `strength_` existierte dort schon, war aber von außen nicht erreichbar.)
- **"none"**: nichts zu skalieren, der Regler ist dann ausgegraut.
- Wird als `Defaults/DitherStrength` gespeichert.

**Verifiziert:**
- Stärke 0 liefert für **alle sechs** Ditherer eine IFF-Datei, die **bit-identisch**
  zu `--dither none` ist; Standard und 100 % sind bit-identisch; 150 bzw. -5 werden
  auf 100 bzw. 0 begrenzt; CLI-Baseline unverändert (0,0126422).
- Wirkung (Mittel über drei Fotos, `scripts/ham_eval.py`), Floyd-Steinberg:

  | Stärke | err_px | err_p99 | err_blur |
  |---|---|---|---|
  | 100 % | 0,01971 | 0,06921 | 0,00170 |
  | 75 % | 0,01774 | 0,06002 | 0,00237 |
  | 50 % | 0,01649 | 0,05549 | 0,00365 |
  | 25 % | 0,01577 | 0,05255 | 0,00504 |
  | 0 % (= none) | 0,01565 | 0,05201 | 0,00635 |

  Der Regler überstreicht damit stufenlos den Kompromiss zwischen "genau im
  Mittel, aber rauschig" (hoher Wert) und "ruhig, aber Banding" (niedriger Wert).
- GUI: Regler aktiv bei Floyd-Steinberg, samt Label ausgegraut bei "none";
  gespeicherter Wert wird geladen; die Statuszeile zeigt nach F5 bei 40 %
  `error=0.0101` – identisch zum CLI-Wert (0,01014), nicht zu dem bei 100 % (0,01264).

### Punkt 4: Zeilenbeginn mit Modify (`--line-start-modify`, standardmäßig aus)

Bisher war das erste Pixel jeder Zeile immer ein Select. Mit der Option darf es
ein Modify sein. Laut ILBM-Spezifikation (Amiga Devices Manual) modifiziert ein
Modify am Zeilenanfang die **Randfarbe (Palettenregister 0)**; die "gehaltene"
Farbe beim Zeilenstart ist also `palette.colors[0]`. `decode_ham6()` startet
deshalb jetzt mit Farbe 0 statt mit Schwarz (ohne die Option unbeobachtbar, da
das erste Pixel dann immer ein Select ist). `HAM6EncoderOptions::line_start_modify`,
`ConversionOptions::line_start_modify`, CLI `--line-start-modify`, GUI-Checkbox
"Allow Modify at line start", gespeichert als `Defaults/LineStartModify`.

**Warum standardmäßig aus – gemessen, nicht vermutet:**
- **Nicht jeder Decoder folgt der Spezifikation.** netpbms `ilbmtoppm` startet jede
  Zeile mit Schwarz (mit einer handgebauten Datei, Farbe 0 = (68,136,204), nachgewiesen:
  Modify-Rot am Zeilenanfang ergibt dort (15,0,0) statt (15,8,12)). Bei schwarzer
  Farbe 0 fallen beide Konventionen zusammen. Auf echter Encoder-Ausgabe mit grauer
  Farbe 0 weicht `ilbmtoppm` in 10–14 Pixeln pro Bild ab (0,01–0,02 %), ausschließlich
  in den 3–5 Zeilen mit Modify-Start.
- **Der Nutzen ist klein und bildabhängig.** Über 6 Bilder (1536 Zeilen) beginnen
  270–323 Zeilen mit Modify (nur 56 bei grauer Farbe 0); der mittlere Fehler ändert
  sich um −0,1 bis −0,4 %. Mit fixierter grauer Farbe 0 sind die vier Fotos unverändert
  (±0,0 %), das synthetische `test_input.png` wird bei aktiver Palettenverfeinerung um
  +6,1 % **schlechter** (die Verfeinerung zählt nur Select-Pixel und landet bei
  geänderten Randpixeln in einem anderen lokalen Optimum). Mit fester Palette
  (`--palette-iterations 0`) verbessert die Option dort dagegen ausnahmslos
  (−0,67 % ohne Dither, −0,53 % mit Floyd-Steinberg) – der Encoder selbst ist
  also korrekt, die Schwankung kommt aus der Verfeinerung.

**Verifiziert:** Option aus → IFF bit-identisch zu vorher; Option an → der unabhängige,
spezifikationskonforme Python-Decoder (`testdata/decode_ham6_independent.py`, jetzt
mit `line_start="border"` als Standard und `--line-start=black` für die
netpbm-Konvention; veralteter Pfad `amiga-image-converter` darin entfernt)
reproduziert `--preview` zu 100 % pixelgleich; GUI-Checkbox rendert deutsch und
englisch und wird aus den gespeicherten Defaults geladen.

### GUI-Layout: HAM-Box kompakter

Die beiden neuen Bedienelemente (Dither-Stärke, Checkbox) hatten die HAM-Box so
verlängert, dass das Fenster auf 898x1122 px wuchs – zu hoch für einen
1080p-Bildschirm. Deshalb stehen jetzt **Dithering | Dither-Stärke** und
**Palette-Quantizer | Farbfokus** nebeneinander (dasselbe FlexGridSizer-Muster wie
die bestehende Zeile Refinement | Chroma-Luminanz). Gemessen (per `xdotool`
aus der Fenstergeometrie): englisch 898x996, deutsch 970x1023 px; deutsche Texte
und der längste Ditherer-Name (`jarvis-judice-ninke`) passen in die halbe Breite.

**Einordnung nachgetragen (like-for-like gemessen):** Der Stand *vor* Block 1 (`bd97fd7`, separat
gebaut) misst mit demselben Skript 898x1030 (englisch) bzw. 970x1057 (deutsch) – exakt so
viel wie der aktuelle Stand mit Dither-Stärke, Zeilenstart-Checkbox, Vorschau-Auswahl und
Lookahead. Das kompakte Layout hat die Fensterhöhe also auf den Ausgangswert zurückgeführt, statt
sie zu vergrößern. Die oben genannten absoluten Werte (996/1023) stammen aus einer etwas
anderen Messung (nach der ersten Konvertierung, andere Konfiguration) und sind nicht direkt
damit vergleichbar; ein früher angenommener Ausgangswert von "rund 910 px" stammte aus einer
anderen Sitzung mit anderer Schrift und war nicht übertragbar.

### Punkt 2: "Checks"-Dither (`checks-lines`, `checks-lines-mixed`, `checks`)

Ein Ordered-Dither mit nur **zwei Phasen**: Die Rundungsschwelle wird pro Kanal um
±¼ 4-Bit-Stufe verschoben. Ein Wert genau zwischen zwei Stufen rundet in der einen
Phase auf, in der anderen ab (Mittel = die halbe Stufe), Werte nahe einer Stufe
(Schwarz, Weiß, alle 17er-Vielfachen) bleiben in beiden Phasen unverändert – das
sind effektiv 31 statt 16 Tonstufen pro Kanal.

- **`checks-lines`** (HAM-spezifisch): Die Phase wechselt von **Zeile zu Zeile** und
  ist innerhalb einer Zeile konstant. In HAM sind Zeilen voneinander unabhängig, nur
  Pixel derselben Zeile hängen aneinander – ein zeilenkonstanter Versatz bringt
  deshalb keine horizontale Pixel-zu-Pixel-Variation ins Bild, die zusätzliche
  Modify-Ketten oder Fringing erzwingen würde.
- **`checks-lines-mixed`**: wie oben, Grün kehrt aber entgegengesetzt zu Rot/Blau um.
  Grün trägt den Großteil der Helligkeit, das Helligkeitsflimmern von Zeile zu Zeile
  sinkt auf ~43 %; dafür leichter Farbstich von Zeile zu Zeile.
- **`checks`**: klassisches Schachbrett (x+y), als Vergleichsbasis.

Technisch braucht das einen neuen Hook `IErrorDiffuser::adjust_target(x, y, target)`
(Standard: `target + bias(x, y)`, damit alle bestehenden Ditherer bit-identisch
bleiben): Der Versatz wirkt pro Kanal auf dem 4-Bit-Gitter, das kann das pixelblinde
`bias()` nicht ausdrücken. Die Ditherer erscheinen automatisch in CLI-Liste und GUI-Auswahl.

**Fehlversuch (dokumentiert, weil lehrreich):** Die erste Fassung addierte ¼ Stufe im
sRGB-Code-Raum zum stetigen Ziel. Ergebnis: `err_px` 0,0428 statt 0,0170 – *dreimal*
schlechter als gar kein Dithering. Eine Zerlegung nach Zeilenparität und Helligkeit
(`/tmp`-Diagnose) zeigte: gerade Zeilen waren mit +16 % im Rahmen, ungerade lagen bei
0,066 und im Mittel um +0,057 L zu hell; das dunkelste Quartil (die schwarzen
Letterbox-Balken) sprang von 0,0000 auf 0,080. Ursache: Der Encoder wählt Kandidaten
nach OKLab-Abstand, und OKLab ist nahe Schwarz extrem steil (Kubikwurzel). Schon ein
Versatz von 0,0167 macht aus Schwarz ein Ziel bei L≈0,109, das näher an Stufe 1
(L≈0,178) als an Schwarz liegt – jede zweite Zeile schwarzer Flächen wurde dunkelgrau.
Die Rundungsschwelle liegt im Encoder also nicht in der Code-Mitte, wie die
Dither-Theorie annimmt. **Korrektur:** erst im 4-Bit-Code-Raum pro Kanal runden
(`floor(v + 0,5 ± 0,25)`), dann dem Encoder das gerundete Gitterziel geben (bei exakten
Gitterwerten bleibt alles unverändert). Danach +24 % `err_px` gegenüber "none" – fast
exakt die theoretisch erwarteten +25 % für ein Zwei-Phasen-Dithering.

**Kalibrierung:** Die Stärke blendet zwischen stetigem Ziel (0) und gerundetem
Gitterziel. Eine Messreihe (vier Fotos, `err_blur`) hat ein flaches Optimum bei
Mischung 0,4–0,6; volles Einrasten (1,0) ist in jeder Metrik schlechter (`err_blur`
0,00527 statt 0,00480, `err_p99` 0,0679 statt 0,0557), weil es dem Encoder die eigene
Wahl des nächsterreichbaren Kandidaten nimmt. "Stärke 100 %" entspricht daher der
Mischung 0,5 (analog zu den empirisch skalierten Bayer-Amplituden).

**Ergebnis** (Mittel über astronaut, coffee, chelsea, rocket; 320x256; `scripts/ham_eval.py`):

| Ditherer | err_px | err_p99 | err_blur | IFF-Bytes |
|---|---|---|---|---|
| none | 0,01590 | 0,05157 | 0,00741 | 50146 |
| floyd-steinberg | 0,02026 | 0,06784 | 0,00166 | 52897 |
| atkinson | 0,01755 | 0,05591 | 0,00278 | 52576 |
| bayer | 0,01876 | 0,05702 | 0,00630 | 51354 |
| blue-noise | 0,01878 | 0,05696 | 0,00631 | 51704 |
| checks (Schachbrett) | 0,01912 | 0,05598 | 0,00634 | 51606 |
| **checks-lines** | 0,01909 | 0,05570 | **0,00480** | 50496 |
| checks-lines-mixed | 0,01914 | 0,05596 | 0,00492 | 50489 |

**Einordnung, ehrlich:**
- Gegenüber den anderen *nicht-propagierenden* Ditherern ist `checks-lines` klar besser:
  `err_blur` −24 % gegenüber Schachbrett/Bayer/Blue-Noise bei gleichem `err_p99`.
  Dass das HAM-spezifische Zeilenmuster zählt, zeigt der Vergleich mit dem
  Schachbrett (gleiche Rundung, andere Anordnung).
- Die IFF-Datei bleibt nahezu so klein wie ohne Dithering (+0,7 %; Bayer +2,4 %,
  Atkinson +4,8 %, Floyd-Steinberg +5,5 %) – passend zur Aussage der ham_convert-
  Dokumentation, die Variante sei gut komprimierbar.
- **Aber:** Nach `err_blur` sind Atkinson (0,00278) und Floyd-Steinberg (0,00166)
  genauer, Atkinson bei niedrigerem `err_px` und gleichem `err_p99`. Die Aussage
  "Floyd-Steinberg ist für HAM schlechter" bestätigt die Messung nicht; sie sagt
  aber nichts über **Optik**: Die Metrik sieht weder Körnung noch Muster. Der
  Zeilenraster von `checks-lines` ist in der Vergrößerung (4×) in glatten Flächen
  sichtbar, bei `checks-lines-mixed` weniger, dort mit leichtem Farbstich. Ob die
  ruhige, rauschfreie Struktur den Genauigkeitsverlust gegenüber Atkinson/FS wert ist,
  bleibt Geschmackssache und sollte im Live-Vorschaufenster entschieden werden.

**Verifiziert:** Der neue Hook ändert bestehende Ditherer nicht (Floyd-Steinberg
bit-identisch zu vorher, Baseline 0,0126422); Stärke 0 ist für alle drei neuen Ditherer
bit-identisch zu `none`; flache Testbilder (exakt auf Stufe 7 → rein (7,7,7), zwischen
zwei Stufen → alternierende Zeilen); GUI mit `checks-lines-mixed` konvertiert
fehlerfrei (Statuswert = CLI-Wert).

### Punkt 8: HAM-Map und Error-Map

Zwei Diagnosebilder (`aic/ham_maps.hpp`), mit denen sich die Wirkung der obigen
Punkte beurteilen lässt:

- **HAM-Map**: ein Pixel pro HAM6-Pixel, Farbe = verwendete Operation –
  **schwarz = Select** (Palettenfarbe), **rot / grün / blau = Modify Rot / Grün / Blau**.
  Lange einfarbige Läufe sind billiges HAM, dichtes Flimmern zwischen den
  Modify-Farben markiert Stellen, die fransen werden.
- **Error-Map**: Graustufen, Helligkeit = OKLab-Abstand zwischen der Leinwand des Encoders
  und dem tatsächlich Reproduzierten, linear von Schwarz (exakt) bis Weiß
  (`kErrorMapFullScale` = 0,10 oder mehr). Die Skala ist **fest**, nicht pro Bild
  normiert, damit Karten verschiedener Einstellungen und Bilder vergleichbar bleiben.

Bedienung: CLI `--ham-map <png>` / `--error-map <png>`; GUI: Auswahlfeld
"Preview | HAM map | Error map" in der Zeile der Interlace-Checkbox (kostet keine
Fensterhöhe), **F6** schaltet zyklisch um (Tooltip, About-Dialog und deutsche
Übersetzung ergänzt). Beide Ansichten laufen auch durch die Interlace-Stauchung.

**Nebenwirkung aus dem QA-Commit behoben:** `PreviewResult::canvas` hätte jedem
Frame einer Animationsvorschau rund 1 MB zusätzlich aufgebürdet. Die Leinwand wird
deshalb nur noch mit `ConversionOptions::keep_canvas` aufgehoben; die GUI setzt das
nur für die Einzelbild-Vorschau. Die Error-Map gibt es folglich nicht während eine
Animation abgespielt wird (die Ansicht fällt dort auf das normale Bild zurück).

**Verifiziert:**
- Die IFF-Dateien bleiben durch die Map-Optionen bit-identisch.
- HAM-Map == unabhängig aus der IFF-Datei geparste Operationen: **pixelgenau** bei drei
  Bildern (astronaut: Select 48 %, Modify R/G/B 15/14/23 %).
- Error-Map: Der aus ihr zurückgerechnete Mittelwert entspricht dem vom Tool
  gemeldeten Fehler auf 0,1 % (Verhältnis 0,999–1,000). Pixelweise gegen eine
  Neuberechnung aus den PNGs: mittlere Abweichung 0,9–1,7 von 255 Stufen; in
  ausgeleuchteten Bereichen höchstens 5–6 Stufen (8-Bit-Rundung der Referenzleinwand),
  größere Abweichungen nur in fast schwarzen Pixeln, wo OKLab so steil ist, dass
  1/255 schon massiv zählt.
- GUI: F6 durchläuft Preview → HAM map → Error map → Preview (englisch und deutsch).
- **Gesamtnachweis Block 1:** Der CLI-Stand vor allen Änderungen (`bd97fd7`, separat
  gebaut) und der aktuelle Stand liefern für 5 Bilder x 7 bisherige Ditherer sowie
  5 Palettenoptionen **bit-identische** IFF-Dateien – Standardverhalten ist unverändert.
  Fenstergröße englisch 898x1009 px.

### Punkt 1: Lookahead (`--lookahead <n>`, standardmäßig aus)

Modify ändert pro Pixel nur **einen** Farbkanal; ein Farbsprung, der sich in mehreren
Kanälen vom gehaltenen Wert unterscheidet, braucht bis zu drei Pixel. Der bisherige
Encoder entscheidet pro Pixel greedy und beginnt erst nach der Kante, nach dem Ziel
zu greifen – das verschmiert. Mit Lookahead wird pro Pixel die beste **Folge** über die
nächsten N Pixel gesucht, nur deren erste Operation übernommen und dann neu gesucht
(Horizontsteuerung), sodass die Modify-Kette schon *vor* der Kante beginnen kann.

**Umsetzung** (`HAM6Encoder::choose_lookahead`): Beam-Search. Pro Ebene werden für jeden
überlebenden Zustand alle 64 Operationen (16 Select, 48 Modify) bewertet; Zustände
mit derselben gehaltenen Farbe werden zusammengelegt (die Zukunft hängt nur von ihr
ab, es überlebt die billigste Folge), die besten K bleiben. Kosten = Summe der
gewichteten OKLab-Abstände über die Folge. Der bisherige Greedy-Pfad bleibt
unverändert; `lookahead_always_beam` erzwingt für Tests die Beam-Suche auch bei Tiefe 1.
Für Fehlerdiffusion sind die Ziele der *folgenden* Pixel nur Näherungen (der Fehler der
dazwischenliegenden Pixel ist noch unbekannt); bei Ordered-Ditherern sind sie exakt.

**Optionen:** CLI `--lookahead <n>` (Tiefe, 1 = greedy = Standard) und `--lookahead-beam <n>`
(1–32, Standard 8); Bibliothek `HAM6EncoderOptions::lookahead`, `lookahead_beam`,
`lookahead_future_weight`, `lookahead_in_refinement`.

**Kalibrierung** (Mittel über astronaut, coffee, chelsea, rocket; Floyd-Steinberg; `scripts/ham_eval.py`,
neu mit `err_edge` = Fehler in den 10 % kantenstärksten Pixeln und Umgebungsvariablen pro Variante):

| Variante | err_px | err_p99 | err_edge | err_blur | Zeit |
|---|---|---|---|---|---|
| Greedy | 0,02026 | 0,06784 | 0,03141 | 0,00166 | 0,1 s |
| Tiefe 2, Beam 4, nur finale Passage | 0,01933 | 0,06411 | 0,03008 | 0,00154 | 0,3 s |
| **Tiefe 3, Beam 8, nur finale Passage** | 0,01912 | 0,06268 | 0,02958 | 0,00150 | 0,7 s |
| Tiefe 4, Beam 8, nur finale Passage | 0,01905 | 0,06265 | 0,02947 | 0,00150 | 1,0 s |
| Tiefe 5, Beam 8, nur finale Passage | 0,01903 | 0,06252 | 0,02940 | 0,00150 | 1,3 s |
| Tiefe 3, Beam 8, alle Passagen | 0,01901 | 0,06302 | 0,02929 | 0,00151 | 2,7 s |

- **Tiefe:** Ab 3 sättigt der Gewinn (Tiefe 4: −0,4 % `err_px` zusätzlich), die Zeit wächst linear.
- **Beam:** 4 holt praktisch alles (Tiefe 3: 0,01904 gegenüber 0,01901 bei 8); 16 und 32
  bringen nichts mehr bei 2–3× Zeit. Beam 1 ist exakt Greedy.
- **Zukunftsgewicht:** 1,0 ist am besten (0,5: 0,01933; 0,75: 0,01909; 1,25: 0,01906).
- **Lookahead auch in der Palettenverfeinerung?** Bringt nur 0,6 % (0,01901 statt 0,01912),
  macht die Konvertierung aber 4× langsamer – deshalb nur in der letzten Passage.

**Wirkung über alle Ditherer** (Tiefe 3, Beam 8, finale Passage; Mittel über vier Fotos):

| Ditherer | err_px | err_p99 | err_edge | err_blur |
|---|---|---|---|---|
| none | 0,01590 → 0,01528 | 0,05157 → 0,04888 | 0,02609 → 0,02502 | 0,00741 → 0,00674 |
| atkinson | 0,01755 → 0,01694 | 0,05591 → 0,05364 | 0,02728 → 0,02615 | 0,00278 → 0,00254 |
| bayer | 0,01876 → 0,01787 | 0,05702 → 0,05393 | 0,02830 → 0,02716 | 0,00630 → 0,00541 |
| checks-lines | 0,01909 → 0,01856 | 0,05570 → 0,05346 | 0,02812 → 0,02719 | 0,00480 → 0,00430 |
| floyd-steinberg | 0,02026 → 0,01912 | 0,06784 → 0,06268 | 0,03141 → 0,02958 | 0,00166 → 0,00150 |

**Einordnung, ehrlich:** Der Gewinn ist **konsistent, aber moderat** – `err_px` −2,8 bis −5,6 %,
`err_p99` −4 bis −7,6 %, `err_blur` −9 bis −14 % – bei rund 7-facher Laufzeit (0,1 → 0,7 s
für 320x256). Im Bildvergleich (Fahnenkante der astronaut.png, 4×) ist die Kante sauberer,
der Schmierstreifen rechts der unteren Fahnenecke in der Fehlerkarte weitgehend verschwunden.
Dass der Effekt kleiner ausfällt, als die ham_convert-Dokumentation vermuten ließe, hat
vermutlich (Hypothese, nicht gemessen) damit zu tun, dass unser Greedy-Encoder ohnehin
schon alle 16 Modify-Werte pro Kanal in OKLab durchprobiert, während dort laut Changelog
der Triple-Modus erst zusätzlich ±1 um den quantisierten Wert prüft.
**Standardmäßig aus**, weil die GUI synchron konvertiert und es weder Abbruch noch
Parallelisierung gibt (Roadmap-Punkt 9): bei einer Animation mit vielen Frames würde sich die
Wartezeit um den Faktor 3–10 verlängern.

**GUI:** Auswahlfeld "Lookahead: off / 2 / 3 / 4 pixels" in der HAM-Box, in der Zeile der
Zeilenstart-Checkbox (kostet keine Fensterhöhe), gespeichert als `Defaults/Lookahead`; Tooltip
und deutsche Übersetzung ("Lookahead: aus / 2 / 3 / 4 Pixel"). Die GUI setzt dieselbe Tiefe, beam 8 und
"nur finale Passage" wie die CLI. Stolperstein beim Übersetzen: gettext hält "3-6% lower"
für eine C-Formatangabe ("% lo"), und ein deutsches "6 % geringer" enthält "% g" – der Tooltip
schreibt deshalb "percent"/"Prozent" aus. Außerdem aus `de.po` 30 veraltete `#~`-Einträge entfernt.

**Verifiziert – erstmals automatisiert** (`ctest`, `tests/test_lookahead.cpp`):
Beam-Suche bei Tiefe 1 == Greedy-Suche (10 Fälle: 5 Ditherer, je mit und ohne
Zeilenstart-Modify); Beam-Breite 1 bei Tiefe 3 == Greedy (5 Ditherer); Determinismus;
auf einem kantenharten Synthetikbild senkt Tiefe 3 den Fehler (none 0,01099 → 0,01091,
Floyd-Steinberg 0,01633 → 0,01594). Zusätzlich: `--lookahead 3` reproduziert die
kalibrierte Zeile exakt; CLI-Baseline unverändert (0,0126422); Gesamtnachweis gegen den
Stand vor Block 1 weiterhin 35 von 35 Kombinationen bit-identisch. **GUI:** Für alle vier
Auswahlstufen speichert Strg+S (Zielverzeichnis per Konfiguration) eine Datei, die
**bit-identisch** zur CLI mit `--lookahead 1…4` ist – die ganze Kette Konfiguration →
Auswahlfeld → `GetOptions()` → Pipeline → Encoder → Writer ist damit geprüft.

## Eingabeprüfung: ungültige Werte werden abgelehnt (A1)

**Anlass.** Beim Durchprobieren der CLI mit Randfällen (siehe Vorschlagsliste A1)
fielen Fehler auf, die still falsche oder kaputte Dateien erzeugten:

| Aufruf (vorher) | Verhalten |
|---|---|
| `--width 0` / `--width -5` | schrieb eine 3-KB-Datei der Größe 0×256 bzw. −5×256, Exit-Code 0 |
| `--width 100000` | 23 s Rechenzeit, im IFF-Header (16 Bit) stand 34464 (= 100000 mod 65536): kaputte Datei |
| `--dither-strength abc` | wurde stillschweigend 0 % |
| `--dither-strength 500` | akzeptiert (Wirkung wie 100 %, Ausgabe zeigte trotzdem „500%“) |
| `--lookahead 99` | 24 s Rechenzeit |
| `--palette-iterations -3` | akzeptiert |
| `--lookahead-beam 0` | wurde stillschweigend zu „greedy“ |
| `--width` ohne Wert | zwei Meldungen: „requires an argument“ **und** „unknown argument“ |

**Jetzt.**

- `renderhamlib/include/aic/limits.hpp` enthält die zulässigen Bereiche an einer
  Stelle. Breite/Höhe 1–8192 (das IFF-Format erlaubt hart 65535, echte Amiga-Modi
  reichen bis etwa 1440×580), Lookahead 0–8, Beam 1–32, Palette-Iterationen 0–64,
  Farbkorrektur-Werte in weiten, aber endlichen Bereichen. Die Grenzen sind absichtlich
  weiter als die GUI-Regler: sie sollen Unsinn verhindern, nicht einschränken.
- `aic::validate_options()` prüft sie, `ConversionPipeline::preview()` ruft sie zuerst
  auf. Damit ist jede Oberfläche (CLI, GUI, eigene Programme) geschützt, bevor
  irgendetwas geladen oder geschrieben wird.
- `write_ham6_ilbm()` und `write_ham6_anim()` weisen Größen außerhalb 1–65535 selbst ab
  (zweite Schutzschicht, falls jemand die Bibliothek ohne Pipeline benutzt).
- Die CLI parst Zahlen streng (`strtol`/`strtof`, ganzer Text muss eine Zahl im Bereich
  sein, NaN/Inf abgelehnt) und meldet z. B.
  `error: --width expects an integer between 1 and 8192, got '100000'`.
  `--fix-color0` prüft jeden Wert auf 0–255, ein fehlender Argumentwert erzeugt genau
  eine Meldung.
- Neu: `renderHAM --version`.

**Verifikation.** `tests/test_validation.cpp` (ctest-Test `validation`) prüft die
Bibliothek und ruft die echte CLI mit 19 ungültigen Aufrufen auf (erwartet: Exit-Code 1,
Meldung nennt die Option, keine Erfolgsmeldung, keine Datei). Dazu zwei gültige Aufrufe
(`--version`, eine Konvertierung mit 321×100 – der BMHD-Header wird gelesen). Mutationsprobe:
mit absichtlich auf 100000 angehobener Grenze schlägt der Test fehl (und die
Writer-Schutzschicht fängt den Fall trotzdem ab, siehe Meldung „cannot be stored“).
Die Baseline `0.0126422` ist unverändert.

## Fenster passt auf kleine Bildschirme (A2)

**Befund (gemessen).** Das Standardfenster war 898×1022 Pixel groß. Auf einem 1366×768-Bildschirm
lag seine Oberkante bei Y = −258: Titelleiste und Menü außerhalb des Bildschirms, nicht
erreichbar. Das Parameter-Panel konnte nicht scrollen, und die Fenstergeometrie wurde nur über
„Save current settings“ gesichert.

**Umsetzung.**

- `ParameterPanel` ist jetzt ein `wxScrolledWindow`. Bei genug Platz bekommt es seine natürliche
  Höhe (kein Scrollbalken, Aussehen unverändert). Reicht der Platz nicht, wird es gekürzt und
  scrollt (Mausrad, GTK-Overlay-Scrollbalken erscheinen beim Überfahren).
- `MainFrame::UpdateParamsPanelHeight()` berechnet bei jeder Größenänderung, wie hoch das Panel
  höchstens sein darf: Fensterhöhe minus Werkzeugleiste, Statuszeile und mindestens 260 px für
  die beiden Vorschau-Flächen, nie unter 120 px.
- Die Standardgröße wird auf den Arbeitsbereich des Bildschirms begrenzt (ohne Taskleisten).
- Eine gespeicherte Geometrie wird nur verwendet, wenn die Titelleiste noch auf einem
  angeschlossenen Monitor liegt; sonst Standardgröße, zentriert. Zu große gespeicherte Größen
  werden auf den Arbeitsbereich gekürzt.
- Neu: Position, Größe und „maximiert“ werden beim Schließen automatisch gespeichert (gleiche
  Schlüssel `Defaults/Window*` wie bisher; „Save current settings“ speichert sie weiterhin mit).

**Verifikation (Xvfb + fluxbox, Screenshots, frische Konfiguration).**

| Bildschirm | Fenster vorher | Fenster jetzt |
|---|---|---|
| 1366×768 | 898×1022 bei Y = −258 (Titelleiste außerhalb) | 898×659, vollständig sichtbar, Panel scrollt |
| 1920×1080 | 898×1022 (gerade so) | 898×971, Panel scrollt ein kurzes Stück |
| 2560×1440 | 898×1022 | 898×1022 (unverändert, kein Scrollbalken) |

Außerdem: Fenster auf 1000×700 gezogen, per Alt+F4 geschlossen, neu gestartet → gleiche
Position und Größe. Gespeicherte Position (5000, 4000) → Standardposition. Gespeicherte Größe
1800×1200 auf 1366×768 → auf 1364×719 gekürzt. Mausrad über dem Panel scrollt bis zum
„Palette“- und „Output“-Bereich.

## EXIF-Drehung und Alpha-Kanal beim Laden (A3)

**Befund (gemessen).** `stbi_load(..., 3)` ignorierte beide Dinge:

- Ein JPEG mit EXIF-Orientierung 6 (Handy-Hochformat, seitlich gespeichert) wurde als Querformat
  gelesen. Testbild 200×100, links rot/rechts blau, Orientierung 6: heraus kam links rot/rechts
  blau statt oben rot/unten blau.
- Der Alpha-Kanal wurde verworfen: ein komplett transparentes Pixel (255, 0, 0, α = 0) wurde
  knallrot. Bei Logos und Icons mit transparentem Hintergrund bestimmte der zufällig gespeicherte
  RGB-Wert des „Nichts“ das Ergebnis.

**Umsetzung.**

- `aic/exif.hpp`: kleiner eigener Parser für das Orientierungs-Tag von JPEG (APP1/Exif,
  Little- und Big-Endian, nie Lesezugriff hinter dem Puffer, jeder Fehlerfall → 1 = unverändert).
  Andere Formate (PNG-eXIf, HEIC, TIFF) werden nicht ausgewertet.
- `aic/transform.hpp`: `ImageTransform` – die acht 90°-Drehungen/Spiegelungen als 2×2-Matrix
  (Diedergruppe D4). Verkettung ist ein Matrixprodukt, `map_rect()` bewegt Rechtecke (später:
  Crop-Boxen und Keyframes) mit. Wird auch vom Drehen/Spiegeln des Quellbilds genutzt.
- `Image::load(path, err, LoadOptions)`: lädt immer als RGBA, legt das Bild über
  `alpha_background` (Standard schwarz = Amiga-Rahmenfarbe, Mischung im kodierten sRGB-Raum wie
  in Browsern und Bildbetrachtern) und wendet danach die EXIF-Orientierung an. Vollständig
  deckende Pixel werden nicht angefasst.
- CLI: `--background R,G,B | #RRGGBB` und `--ignore-exif`.
- GUI: *Settings → Hintergrund für transparente Bilder* (Farbwähler, gespeichert als
  `TransparentBackground`); eine Änderung lädt das aktuelle Bild neu. EXIF wird immer angewendet.
  Alle Konvertierungen der GUI laufen über `MainFrame::MakeOptions()`.

**Verifikation.**

- `tests/test_orientation_alpha.cpp` (ctest `orientation_alpha`): alle 8 EXIF-Werte in beiden
  Byte-Reihenfolgen, ungültige Werte/Typen, JPEG ohne EXIF, jede Abschneidung eines gültigen
  Blocks (keine Zugriffe hinter dem Ende); die 8 Transformationen gegen ihre Bedeutung (z. B.
  „6 = oben links wandert nach oben rechts“), alle 64 Verkettungen, `inverse()` und `map_rect()`
  (Ausschnitt vor oder nach der Transformation ergibt dieselben Pixel); echte Testdateien
  (`testdata/exif_orient6.jpg`, `exif_orient3.jpg`, `alpha_samples.png`, erzeugt von
  `testdata/make_orientation_alpha_images.py`) inklusive Alpha über Schwarz und über Weiß.
- CLI: Das 200×100-Testbild kommt jetzt als 100×200 mit rot oben/blau unten an, mit
  `--ignore-exif` wie zuvor. Das transparente rote Pixel wird schwarz, mit `--background
  255,255,255` weiß, mit `#204080` die angegebene Farbe; ein ungültiger Wert wird abgelehnt.
- GUI unter Xvfb (Screenshots): transparentes PNG mit grün „versteckten“ Pixeln erscheint auf
  Schwarz (Standard) bzw. auf Blau (`TransparentBackground=#2040A0`), die halbtransparente weiße
  Fläche wird entsprechend gemischt; das EXIF-Bild erscheint aufrecht (rot oben, blau unten)
  in Quelle und Vorschau.
- Regression: Baseline `0.0126422` unverändert; die vier Fotos (`astronaut`, `chelsea`,
  `coffee`, `rocket`) ergeben **bit-identische** IFF-Dateien wie vor der Änderung.

## Quellbild drehen und spiegeln (CLI und GUI)

**Idee (vom Nutzer).** Das Quellbild soll sich drehen und spiegeln lassen, ohne dass man die Datei
vorher in einem anderen Programm bearbeiten muss.

**Umsetzung.**

- `ConversionOptions::source_transform` (`aic::ImageTransform`, siehe A3) wird in
  `ConversionPipeline::preview()` direkt nach dem Laden angewendet, also nach der automatischen
  EXIF-Drehung und **vor** dem Crop. Crop-Rechtecke gelten damit in den Koordinaten des gedrehten
  Bildes, also dessen, was man sieht.
- CLI: `--rotate <0|90|180|270>` (im Uhrzeigersinn, auch −90 usw.), `--flip-h`, `--flip-v`.
  Reihenfolge: erst spiegeln, dann drehen (dieselbe Reihenfolge, in der die GUI-Knöpfe sich
  verketten).
- GUI: vier Knöpfe unter dem Quellbild (links drehen, rechts drehen, horizontal spiegeln,
  vertikal spiegeln) und Tastenkürzel **Strg+L**, **Strg+R**, **Strg+Umschalt+H**,
  **Strg+Umschalt+V** (stehen auch im About-Dialog). Die Symbole sind Alphamasken
  (`TransformIcons.hpp`, erzeugt von `scripts/make_transform_icons.py`), in der Textfarbe des
  Themes eingefärbt; Pfeil-Zeichen im Knopftext blieben in der Testumgebung leer.
  Ein neu geöffnetes Bild beginnt wieder aufrecht. Die Vorschau wird geleert (sie zeigt die alte
  Ausrichtung).
- **Crop-Box und Animations-Keyframes drehen mit** (`ParameterPanel::ApplySourceTransform`), damit
  das gewählte Bildfeld dasselbe bleibt. Eine Besonderheit: ist „Lock aspect ratio“ aktiv, bleibt
  die Box exakt abgebildet (aus 8:5 wird 5:8); das Verhältnis greift erst wieder bei der nächsten
  Eingabe von Breite/Höhe.

**Verifikation.**

- CLI an einem 80×40-Bild mit vier Farben (Ecken R B / G Y): `--rotate 90` → G R / Y B, `180` →
  Y G / B R, `270` und `-90` → B Y / R G, `--flip-h` → B R / Y G, `--flip-v` → G Y / R B,
  `--flip-h --rotate 90` → Y B / G R; `--rotate 45`, `abc`, `450` werden abgelehnt.
- ctest: `orientation_alpha` prüft dieselben Fälle über die Pipeline (`source_transform`),
  `validation` die abgelehnten Aufrufe.
- GUI unter Xvfb (Screenshots, echte Tastendrücke): Strg+R macht aus 640×400 ein 400×640-Bild,
  nach F5 ist auch die Vorschau gedreht. Die Crop-Box (100×100 bei 0,0) wandert mit: nach Strg+R
  steht sie bei X = 300 über derselben Bildstelle, nach Strg+Umschalt+H bleibt sie über der
  Bildstelle, nach erneutem Spiegeln und Strg+L ist wieder X = 0. Zwei Keyframes (X = 200 und
  X = 400) wurden nach Strg+R zu (300, 200) und (300, 400) – im Keyframe-Auswahlfeld nachgesehen.
- Regression: Baseline `0.0126422`, die vier Fotos sind **bit-identisch** zum Stand davor.

## kmeans und Lookahead 3 als Standard (B1 + B2)

> **Stand heute:** kmeans ist weiter Standard; der **Lookahead ist wieder aus** (siehe „Lookahead wieder aus“ weiter unten). Die Messung unten gilt für `--lookahead 3`.

**Anlass (gemessen, `scripts/ham_eval.py`, vier Fotos, 320×256, `--fit fill`).**

| Variante | err_px | err_p99 | err_edge | err_blur | Bytes | Zeit |
|---|---|---|---|---|---|---|
| median-cut, greedy (bisheriger Standard) | 0.02391 | 0.07146 | 0.03454 | 0.00193 | 61793 | 0,2 s |
| kmeans, greedy | 0.02341 | 0.06780 | 0.03269 | 0.00186 | 62553 | 0,2 s |
| **kmeans + Lookahead 3 (neuer Standard)** | **0.02246** | **0.06288** | **0.03099** | **0.00172** | 62485 | 0,8 s |

Gegenüber dem bisherigen Standard: mittlerer Pixelfehler −6,1 %, 99. Perzentil −12,0 %, Kantenfehler
−10,3 %, Fehler nach Weichzeichnung −10,9 %, Dateigröße +1,1 %. kmeans war schon allein auf **allen
vier** Fotos besser als median-cut (−2,1 % im Mittel); octree war im Mittel 1,5 % schlechter als
median-cut. Auf dem synthetischen `test_input.png`: −2,7 %.

**Was sich ändert.**

- `ConversionOptions`: `quantizer_name = "kmeans"`, `lookahead = 3`. Die Bausteine
  (`HAM6EncoderOptions`, `lookahead = 1`) bleiben unverändert; geändert sind nur die Vorgaben der
  fertigen Pipeline. CLI und GUI übernehmen sie (GUI: Quantizer „kmeans“, „Lookahead: 3 pixels“;
  wer in der GUI bereits „Save current settings“ benutzt hat, behält seine gespeicherten Werte).
- Die alte Konfiguration bleibt mit `--quantizer median-cut --lookahead 1` reproduzierbar.
- **Neue Baseline:** `test_input.png` mit Standardoptionen = **`0.0123002`** (vorher `0.0126422`).
  Alle früheren Baseline-Angaben in diesem Dokument beziehen sich auf median-cut + greedy.
- Preis: etwa 6-fache Rechenzeit (320×256: 0,12 s → 0,76 s, 640×512 Lace: 0,45 s → 2,6 s). Bei
  Animationen multipliziert sich das mit der Bildzahl (300 Frames: etwa 36 s → 3,8 min, hochgerechnet).
  Der Hintergrund-Thread mit Abbrechen und parallelen Frames ist inzwischen da (siehe „Konvertierung im
  Hintergrund“ weiter unten).

**Verifikation.**

- **Alte Einstellungen, explizit gesetzt, ergeben exakt die alten Dateien:** 200 Kombinationen
  (5 Bilder × alle 10 Ditherer × 4 Varianten: Standard, `--dither-strength 60`, `--line-start-modify`,
  `--palette-iterations 0`) – Vergleich `cmp` gegen die CLI vor der Änderung, 0 Abweichungen.
- **Die neuen Standardwerte sind genau kmeans + Lookahead 3:** 15 Fälle (5 Bilder × Standard, 640×512,
  Atkinson mit Stärke 70) gegen die alte CLI mit explizit `--quantizer kmeans --lookahead 3`,
  0 Abweichungen. Es hat sich also nichts außer den Vorgaben geändert.
- Die Messtabelle oben stammt aus dem Lauf mit den neuen Standardwerten (Zeile „neu“) gegen
  `--quantizer median-cut --lookahead 1`.
- GUI-Screenshot: Quantizer „kmeans“ und „Lookahead: 3 pixels“ sind vorausgewählt, F5 liefert die
  Vorschau samt Palette.
- ctest: alle drei Tests grün.

## Konvertierung im Hintergrund, Abbrechen, parallele Animation (A4)

**Anlass.** Mit Lookahead 3 als Standard dauert eine Konvertierung spürbar (640×512: etwa 3 s), eine
Animation mit vielen Frames Minuten. Bisher lief alles synchron auf dem GUI-Thread (`wxYield()` als
Notlösung): das Fenster reagierte nicht, es gab keinen Abbruch, und Frames wurden einer nach dem
anderen gerechnet.

**Was sich ändert.**

- **Fortschritt und Abbruch in der Bibliothek.** `ConversionOptions::progress` (`std::function<bool(float)>`)
  wird vor dem Encoder einmal und danach am Anfang jeder Bildzeile jedes Durchlaufs aufgerufen
  (Bruchteil 0..1); `false` bricht ab. Der Encoder wirft dann intern `OperationCancelled`, die Pipeline
  fängt es und setzt `PreviewResult::cancelled` bzw. `ConversionResult::cancelled` (kein Fehler,
  nichts wird geschrieben). Die Fortschrittsanzeige gewichtet die Durchläufe nach ihren Kosten
  (gemessen: ein Lookahead-3-Durchlauf ≈ 40 Greedy-Durchläufe), damit der Balken gleichmäßig läuft
  und nicht erst durch die schnellen Verfeinerungsdurchläufe schleicht und dann stehen bleibt. Ohne
  Callback (CLI, Tests) ändert sich nichts, auch nicht an der Laufzeit.
- **Quelle einmal laden.** `ConversionPipeline::preview()` ist jetzt `load_source()` + `preview_source()`.
  Eine Animation lädt das Bild einmal, nicht einmal pro Frame (bei einem großen JPEG spürbar).
- **`render_sequence()` (`aic/sequence.hpp`).** Rendert die Frames einer Animation auf mehreren Threads.
  Frame 0 läuft zuerst allein, weil seine Palette für alle anderen gesperrt wird; danach verteilen sich die
  übrigen Frames auf die Threads (`threads = 0` = ein Thread pro Hardware-Thread). Hat der Aufrufer schon
  eine Palette gesperrt, läuft alles von Anfang an parallel. Ergebnis, Fehler und Abbruch kommen als
  `SequenceResult`; jedes fertige Frame geht an `on_frame` (Aufrufe nie gleichzeitig, der Aufrufer darf
  also Dateien schreiben oder einen vorab dimensionierten Vektor füllen). Bei mehreren Fehlern wird der
  mit dem niedrigsten Frame-Index gemeldet, damit das Ergebnis nicht vom Thread-Timing abhängt.
- **GUI (`BackgroundJob`).** Vorschau (F5), Speichern, Animations-Export und Animations-Vorschau laufen
  auf einem Arbeits-Thread, der kein wx-Objekt anfasst; Ergebnisse kommen über `CallAfter` zurück, und
  veraltete Nachrichten eines abgebrochenen Jobs werden verworfen. In der Statuszeile erscheinen
  Fortschrittsbalken und **Abbrechen** (Esc). Das Fenster bleibt bedienbar (Zoomen, Ansicht wechseln).
  Beim Export zeigt die Vorschau das jeweils zuletzt fertige Frame als Live-Fortschritt.
- **Regeln bei einer zweiten Anforderung.** Eine laufende *Vorschau* wird ersetzt (neueste Anforderung
  gewinnt, z. B. F5 nach einer Slider-Änderung). Ein laufendes *Speichern/Export* wird nie von einer
  Anforderung unterbrochen: Öffnen, Drehen, Spiegeln, Speichern, F5 werden mit dem Hinweis „Export
  running - Esc cancels it.“ abgelehnt; nur Abbrechen stoppt es. Beenden während eines Exports fragt
  nach. Ändert man Keyframes oder das Bild, während die Animations-Vorschau noch gerechnet wird, wird
  sie verworfen. Der Knopf heißt beim Aufbau „Stop preview“, ein Klick oder F5 bricht ab.
- Die Statuszeile kürzt zu lange Texte mit „…“, damit sie nicht über den Fortschrittsbalken läuft.
- Build: `renderhamlib` linkt `Threads::Threads`. Ein MinGW-Cross-Build braucht für `std::thread` das
  POSIX-Thread-Modell (`x86_64-w64-mingw32-g++-posix`).

**Verifikation (alles gemessen, Sandbox mit 2 Kernen).**

- Neuer Test `sequence` (80 Prüfungen): Fortschritts-Callback ändert das Ergebnis nicht (bitgleich),
  Werte liegen in 0..1 und laufen nie rückwärts, mindestens ein Aufruf pro Zeile und Durchlauf; Abbruch
  bei 50 % und vor dem Encoder; `convert()` schreibt bei Abbruch keine Datei; `preview()` ==
  `load_source()` + `preview_source()` (auch mit gedrehter Quelle).
- **Parallel == sequentiell, Byte für Byte:** 9 Frames (Schwenk, das letzte teilweise außerhalb des
  Bildes), Floyd-Steinberg und Bayer, mit 1, 2 und 4 Threads gegen die alte Schleife
  (`preview()` je Frame, Palette von Frame 0 gesperrt); ebenso mit vom Aufrufer gesperrter Palette und
  erzwungener Farbe 0. Jedes Frame genau einmal, `on_frame` nie überlappend.
- Abbruch einer Sequenz (1 und 3 Threads) stoppt lange vor dem 40. Frame, ein Verbraucher-Fehler
  (z. B. volle Platte) und ein ungültiger Zuschnitt in der Mitte werden mit Frame-Index gemeldet.
- **ThreadSanitizer** auf dem ganzen Test: 0 Meldungen.
- **Geschwindigkeit** (`render_sequence`, 320×256, Lookahead 3, 24 Frames): 16,98 s mit einem Thread,
  **8,76 s mit zwei** (1,94-fach). 640×512, 8 Frames: 21,9 s → 14,3 s (Frame 0 läuft allein; mit mehr
  Frames nähert sich der Gewinn der Kernzahl).
- **GUI unter Xvfb/xdotool:** Balken läuft (28 % → 38 %), während der Konvertierung zoomt ein Klick auf
  „-“ (Quelle 100 % → 19 %); Esc → „Cancelled.“; vollständiger Lauf → „640×512 error=0.0173 LACE=on“.
  **Speichern aus der GUI == CLI**, byteidentisch. **Animations-Export aus der GUI (5 Frames,
  640×512, Lookahead 4) == CLI-Folge mit `--save-palette`/`--load-palette`**, alle fünf Dateien
  byteidentisch, in 13 s. Abbruch mitten im Export: danach wird nichts mehr geschrieben. Strg+O während
  des Exports: Hinweis, kein Dateidialog. Strg+Q während des Exports: Rückfrage, „Yes“ beendet den
  Prozess sauber. F5 während des Vorschau-Aufbaus bricht ab (Knopf wieder „Play preview“). Anim-File:
  gültige FORM ANIM mit 5 ILBM-Frames.
- **Regression:** CLI-Ausgabe von Commit `e593597` und neuem Stand auf fünf Optionssätzen (Standard,
  Bayer + greedy, median-cut + Lookahead 2, Crop + Drehen + Spiegeln + Blue-noise) byteidentisch.
- ctest: alle vier Tests grün.

## Animations-Tempo: eine Einstellung für Vorschau und Anim-File (A5)

**Anlass.** Die Vorschau lief fest mit 10 Bildern pro Sekunde (Timer 100 ms), der Anim-File-Export schrieb
fest `reltime = 2` Jiffies, also 30 Bilder pro Sekunde. Die Vorschau zeigte damit ein anderes Tempo als
die Datei später hat, und das Tempo ließ sich nirgends ändern.

**Was sich ändert.**

- Neue Auswahl **„Playback speed“** in der Animations-Box (unter „Frames between keyframes“):
  60, 30, 20, 15, 12, 10, 6, 5, 3, 2 oder 1 fps. Angeboten werden nur Werte, die 60 teilen, weil eine ANIM
  die Bildzeit in Jiffies zu 1/60 s zählt und nur diese exakt darstellbar sind.
- **Dieselbe Einstellung** bestimmt den Takt der Vorschau (`1000 / fps` ms, ein laufender Vorschau-Timer
  wird beim Umschalten sofort neu getaktet) und die ANHD-`reltime` im Anim-File. Nummerierte Einzelbilder
  haben kein Tempo (Tooltip sagt das).
- `aic::anim_jiffies_for_fps(fps)` (`aic/anim_writer.hpp`): `60/fps` auf ganze Jiffies gerundet, mindestens 1,
  fps auf 1..60 begrenzt. `AnimWriteOptions::reltime_jiffies` bleibt wie es war (Standard 2).
- „Save current settings“ merkt sich das Tempo (`Defaults/AnimFps`, als fps-Wert gespeichert, nicht als
  Listenindex).
- **Standard: 30 fps.** Das ist das, was Exporte schon immer hatten; gespeicherte Anim-Dateien ändern sich
  also nicht. **Die Vorschau läuft dadurch standardmäßig schneller als bisher (30 statt 10 fps).** Wer das alte
  Tempo mag, wählt 10 fps und speichert die Einstellung.

**Verifikation.**

- Neuer Test `anim_speed`: Tabelle fps → Jiffies für alle 12 Teiler von 60, 25 fps rundet auf 30, Werte
  außerhalb 1..60 werden begrenzt; die geschriebene Datei enthält in **jedem** ANHD-Chunk genau die
  erwartete `reltime` (60, 30, 10, 2, 1 fps); Standardoptionen schreiben weiterhin 2.
- GUI unter Xvfb/xdotool: Anim-File-Export bei 60 / 30 / 5 / 1 fps liefert `reltime` **1 / 2 / 12 / 60** in allen
  vier ANHD-Chunks. Vorschau mit 5 Bildern (Aufbau eingeschlossen): 2 fps 2,8 s, 5 fps 1,3 s, 60 fps 0,4 s
  (Aufbau etwa 0,3 s, dazu 4 Takte; die Zeiten sind per Screenshot-Abfrage gemessen, also auf etwa 0,3 s genau).
  Umschalten per Tastatur und Maus funktioniert, „Save current settings“ schreibt `AnimFps=10`, nach dem Neustart
  steht 10 fps da; ohne gespeicherten Wert 30 fps.
- ctest: alle fünf Tests grün.

## Dither-Muster wandert mit dem Bild: ruhige Animationen mit Bayer und Blue-noise (B4)

**Anlass.** Bei einem Schwenk (Ken-Burns-Effekt) bleibt das Muster eines geordneten Ditherers am Bildschirm
stehen, während der Bildinhalt darunter wandert. Jeder Punkt des Musters trifft in jedem Frame auf anderen
Inhalt: das Dither „kocht“. Gemessen an zwei Frames eines Schwenks um 3 Pixel (Mittelwert des Unterschieds
im überlappenden Bereich, Skala 0–255): Bayer **8,84**, Blue-noise **8,98**, Floyd-Steinberg 7,29, ohne Dither
0,00. Das Bild selbst ist in beiden Frames dasselbe, der gesamte Unterschied ist Dither-Flimmern.

**Idee.** Das Muster wird auf das Bild „gedruckt“ statt auf den Bildschirm: Frame für Frame verschiebt sich der
Ursprung des Musters genau so, wie sich der Zuschnitt über das Quellbild bewegt hat. Dann liegt jeder Punkt
bei jedem Frame auf demselben Bildinhalt und wandert mit ihm mit.

**Was sich ändert.**

- `IErrorDiffuser::set_pattern_origin(ox, oy)` (`aic/dither.hpp`): verschiebt das Muster so, dass Pixel (x, y)
  den Schwellwert bekommt, den das unverschobene Muster bei (x+ox, y+oy) hat. Umgesetzt für `bayer`,
  `blue-noise`, `checks`, `checks-lines`, `checks-lines-mixed` (negative Ursprünge funktionieren, es wird mit
  `& 7` / `& 1` maskiert, nicht mit `%`). Die Fehlerdiffusions-Verfahren (Floyd-Steinberg, Atkinson, Sierra-lite,
  Jarvis-Judice-Ninke) ignorieren den Aufruf: ihr Rauschen entsteht aus dem Bildinhalt, es gibt kein Muster,
  das man verschieben könnte.
- `ConversionOptions::anchor_dither_to_source` (Standard **aus**, damit einzelne Konvertierungen unverändert
  bleiben). Eingeschaltet berechnet die Pipeline den Ursprung aus Zuschnittbox, Skalierung und Rand
  (`ox = lround(crop.x · skaliert_breite / crop.breite) − offset_x`, ebenso y). Dafür gibt es zwei kleine
  Hilfsfunktionen, die auch von den bisherigen Pfaden benutzt werden: `resolve_crop_box()` (`aic/crop.hpp`) und
  `resize_geometry()` (`aic/resize.hpp`); `crop_image()` und `resize_image()` rechnen jetzt damit, damit
  Zuschnitt, Skalierung und Muster-Ursprung garantiert dieselbe Geometrie verwenden.
- `SequenceOptions::anchor_dither` (Standard **an**): `render_sequence()` schaltet es für jeden Frame ein. Damit
  sind Animationsvorschau, Anim-File und nummerierte Einzelbilder in der GUI **automatisch** ruhiger, sobald ein
  Bayer-, Blue-noise- oder Checks-Dither gewählt ist; es gibt dafür keinen Schalter in der Oberfläche.
- CLI: `--anchor-dither` (Standard aus). Wirkt mit `--crop`; für Skripte, die einen Schwenk selbst als Folge
  von Zuschnitten erzeugen. Ohne `--crop` und mit Rand-/Fit-Verschiebung gleich null ändert es nichts.

**Grenzen (ehrlich).**

- **Exakt nur für Schwenks.** Bei einem Zoom bewegt sich der Inhalt an jeder Stelle mit anderer Geschwindigkeit,
  das Muster kann nur einem Punkt (der linken oberen Quellecke) folgen. Dort ist das Flimmern nicht schlechter
  als vorher, aber auch nicht beseitigt.
- Der Ursprung wird auf ganze Pixel gerundet. Bei verkleinerten Frames wandert der Inhalt in Bruchteilen von
  Pixeln; die Rundung ist im Test mit einer 2:1-Verkleinerung geprüft (siehe unten).
- Fehlerdiffusion bleibt, wie sie ist: bei gleichem Schwenk 7,3 statt 0,02. Wer eine ruhige Animation will,
  nimmt jetzt Bayer oder Blue-noise.

**Verifikation.**

- Neuer Test `anchor_dither` (37 Prüfungen):
  1. Für alle fünf geordneten Ditherer ist der Bias bei (x, y) mit Ursprung (ox, oy) **bitgleich** dem
     unverschobenen Bias bei (x+ox, y+oy), mit acht verschiedenen Ursprüngen einschließlich negativer. Ein
     Ditherer, der den Aufruf ignoriert, fällt durch (Gegenprobe: Ursprung (1, 1) ändert das Muster; beim
     Schachbrett nicht, weil (1, 1) dort eine ganze Periode ist). Die Fehlerdiffusions-Verfahren liefern mit und
     ohne Aufruf dieselben Werte.
  2. Schwenk-Messung in der Pipeline, 1:1 und mit 2:1-Verkleinerung (jeweils 3 Pixel Versatz im Bild; im
     zweiten Fall 6 Quellpixel). Unterschied im überlappenden Bereich, Skala 0–255:

     | Ditherer | Muster am Bildschirm | Muster am Bild | 2:1 am Bildschirm | 2:1 am Bild |
     |---|---|---|---|---|
     | bayer | 8,84 | **0,02** | 7,82 | **0,01** |
     | blue-noise | 8,98 | **0,00** | 7,88 | **0,01** |
     | checks-lines (nur y) | 0,00 | 0,00 | 0,00 | 0,00 |
     | floyd-steinberg, atkinson | unverändert (Flag ändert kein Pixel) | | | |

     Die Prüfung verlangt mindestens Faktor 10; gemessen wird mehr als Faktor 300. Ohne Dither ist der
     Unterschied genau 0,000, also kommt der Rest wirklich nur vom Dither und nicht von der HAM-Kette.
     Zur Einordnung: Schwenk um genau eine Bayer-Periode (8 Pixel) gibt auch ohne Verankerung 0,03, das Muster
     stimmt dann von selbst überein.
     Der Test misst mit Lookahead 0 (schnell, eindeutig). Zusätzlich von Hand mit dem Standard-Lookahead 3,
     Versatz 3 und 5 Pixel: Bayer 8,27 / 8,33 → **0,003 / 0,005**, Blue-noise 8,41 / 7,11 → **0,001 / 0,000**,
     Floyd-Steinberg 6,49 / 6,44 unverändert, ohne Dither 0,000.
  3. Alte Ergebnisse bleiben gültig: Voreinstellungen (`false` / `true`), ohne Zuschnitt und mit Zuschnitt an der
     Quellecke bitgleich mit und ohne Flag, Zuschnitt um ganze Perioden verschoben bitgleich, ein Zuschnitt bei
     (37, 22) dagegen verschieden (damit das Flag nachweislich etwas tut; (37, 21) hätte beim Schachbrett eine
     gerade Summe und würde das Muster nicht bewegen).
- Test `sequence` erweitert: Die Referenzschleife setzt dasselbe Flag, `render_sequence` mit 1, 2 und 4 Threads
  bleibt für Floyd-Steinberg **und** Bayer bitgleich zur sequenziellen Referenz. Zusätzlich: `anchor_dither = false`
  ergibt Frames, die exakt denen unverankerter Einzelkonvertierungen entsprechen, und mit `true` sind die
  Bayer-Frames wirklich andere.
- Regression gegen den vorigen Commit (CLI): 70 Kombinationen aus sieben Ditherern, Lookahead 0/3, Zuschnitten
  (auch über den Rand hinaus) und Fit-Modi stretch/fit/fill: Ausgabedateien **byte-identisch**; `--anchor-dither`
  ohne `--crop` ebenfalls identisch, mit `--crop` bei Bayer verschieden, bei Floyd-Steinberg identisch.
  Die Umstellung von `crop_image()`/`resize_image()` auf die gemeinsamen Geometriefunktionen wurde zusätzlich an
  45 Optionskombinationen gegen das alte Verhalten geprüft.
- ctest: alle sechs Tests grün.

## Rendering-Box, Lookahead wieder aus, Wahrnehmungsmetriken (B6), Symbol-Buttons

Vier zusammengehörige Änderungen aus einer Runde.

### Box „Animation“ heißt jetzt „Rendering“

In dieser Box sitzt neben den Animationseinstellungen auch „Show HAM preview“, das ein einzelnes Bild
rendert. Die Überschrift „Animation“ stimmte für die Bilderzeugung also nicht. Sie heißt jetzt
**„Rendering“** (deutsch ebenfalls „Rendering“); die Meldung „Animation braucht mindestens 2 Keyframes …“ verweist
auf den Bereich „Rendering“. Nur die Beschriftung ändert sich, nichts am Verhalten.

### Lookahead standardmäßig aus (nimmt den Lookahead-Teil von B2 zurück)

Der Lookahead ist wieder **aus**: Bibliothek (`ConversionOptions::lookahead = 0`), CLI (`--lookahead` ohne Angabe)
und GUI (Auswahl „Lookahead: off“). **kmeans bleibt Standard-Quantizer** (der Teil B1 ist davon nicht berührt).
Grund: jede Vorschau und jedes Animationsframe dauert mit Lookahead 3 etwa das Vierfache (320×256 mit
Floyd-Steinberg: 0,2 s aus, 0,9 s mit Lookahead 3). Wer die rund 2 bis 4 Prozent geringeren Fehler will, schaltet
ihn ein (`--lookahead 3`, GUI-Auswahl neben „Allow Modify at line start“).

- Ein in der GUI mit „Save current settings“ gespeicherter Wert (`Defaults/Lookahead`) bleibt erhalten; nur wer nie
  gespeichert hat, sieht jetzt „aus“.
- `scripts/ham_eval.py`-Varianten, die sich auf den Standard verlassen, laufen jetzt ohne Lookahead; ein
  Vergleich „mit Lookahead“ braucht `--lookahead 3` ausdrücklich. Die Zahlen im Abschnitt „kmeans und Lookahead 3
  als Standard (B1 + B2)“ gelten weiter, nur eben für `--lookahead 3` statt für den Standard.
- **Regression:** 48 Dateien (6 Ditherer × 4 Optionssätze × 2 Richtungen) byte-identisch: neuer Standard gegen das
  alte Programm mit `--lookahead 0`, neues `--lookahead 3` gegen den alten Standard.

### Wahrnehmungsmetriken: weichgezeichneter Fehler und SSIM (aus B6)

**Anlass.** Der gemeldete „mean OKLab error“ bewertet jedes Pixel für sich und bestraft Dithering damit zwingend:
ein Dither tauscht die Genauigkeit des einzelnen Pixels gegen einen guten Mittelwert der Umgebung, und die Zahl
sieht nur die erste Hälfte dieses Tauschs. Mit nur dieser Zahl erscheint „kein Dither“ in der Regel als bestes
Ergebnis, auch wenn ein gedithertes Bild beim Betrachten besser wirkt.

**Neu:** `aic/quality_metrics.hpp`, `compute_quality_metrics(canvas, decoded)` liefert drei Zahlen, alle in OKLab:

| Zahl | Bedeutung | belohnt / bestraft |
|---|---|---|
| `mean_error` | mittlerer OKLab-Abstand pro Pixel (der bisherige Wert) | bestraft Dithering |
| `blurred_error` | mittlerer Abstand, nachdem **beide** Bilder mit Gauß (σ = 1,5 px) weichgezeichnet wurden, also wie Auge und Monitor benachbarte Pixel mischen | belohnt Dithering |
| `ssim` | strukturelle Ähnlichkeit (Wang et al. 2004) der OKLab-Helligkeit, 11×11-Gaußfenster σ = 1,5, C1 = 0,01², C2 = 0,03², Mittel über das Bild, gespiegelte Ränder; 1 = identisch | bestraft Dither-Rauschen (zählt als fremde Struktur), erkennt verschmierte Kanten und verlorene Textur |

`blurred_error` hat dieselbe Definition wie `err_blur` in `scripts/ham_eval.py`, die Zahlen sind vergleichbar.

**Wo man sie sieht.**

- CLI: Die Ergebniszeile endet jetzt auf `…, mean OKLab error=0.0125585, blurred error=0.00131301, SSIM=0.904652)`.
  Die alte Angabe steht unverändert zuerst, Skripte, die sie auslesen, funktionieren weiter.
- GUI: Statuszeile nach der Konvertierung, z. B. `320×256  Fehler=0,0126  weich=0,0013  SSIM=0,905  LACE=aus`.
  Der Tooltip der Statuszeile erklärt die drei Zahlen in zwei, drei Sätzen.
- `scripts/ham_eval.py`: neue Spalte `ssim` (unabhängige NumPy-Rechnung auf den PNGs).
- Bibliothek: `ConversionOptions::quality_metrics` (Standard **aus**), Ergebnis in `PreviewResult::metrics` und
  `ConversionResult::metrics` (`valid == false`, wenn nicht berechnet). CLI und GUI-Einzelbildvorschau schalten sie
  ein; **Animationsframes nie** (`render_sequence` erzwingt aus, niemand sieht sie dort).
- Kosten (Release, gemessen): etwa 50 ms bei 320×256, 120 ms bei 352×560, 220 ms bei 640×512; das ist rund ein
  Drittel einer Konvertierung ohne Lookahead. Das Ergebnis der Konvertierung ändern sie nicht.

**Was die Zahlen zeigen** (`test_input.png`, 320×256, Lookahead aus, `scripts/ham_eval.py`):

| Variante | err_px | err_blur | ssim |
|---|---|---|---|
| floyd-steinberg | 0,01256 | **0,00133** | 0,9045 |
| none (kein Dither) | **0,00978** | 0,00586 | **0,9650** |
| bayer | 0,01324 | 0,00520 | 0,9063 |
| floyd-steinberg + Lookahead 3 | 0,01230 | 0,00126 | 0,9070 |

Die drei Zahlen widersprechen sich hier absichtlich: Pro Pixel gewinnt „kein Dither“, nach der Weichzeichnung
gewinnt Floyd-Steinberg mit Abstand (4,4-mal kleiner), und die SSIM sieht das Dither-Rauschen als Strukturverlust
und liegt bei „kein Dither“ am höchsten. **Das ist keine Gesamtnote**, die sich aus den dreien errechnen ließe;
welcher Wert zählt, hängt vom Bild und vom Betrachtungsabstand ab. Der Gewinn des Lookahead zeigt sich bei allen
dreien (−2,1 % / −5,3 % / +0,3 %), bei der SSIM nur schwach. Umgesetzt ist die Messung und Anzeige;
den Encoder selbst steuert keine der neuen Zahlen.

**Verifikation.**

- Neuer Test `quality_metrics` (alle Prüfungen grün):
  1. **Unabhängige Gegenrechnung:** für drei deterministische Float-Bildpaare (23×17, 40×31 und 6×4, Letzteres kleiner
     als der Weichzeichner-Radius, die Spiegelung muss mehrfach greifen) stimmen mittlerer Fehler,
     weichgezeichneter Fehler und SSIM auf 1e-5 mit einer NumPy-Rechnung in Float64 überein, die nicht aus diesem Code
     stammt (`ham_eval.py`-Weichzeichner plus Lehrbuch-SSIM).
  2. Fälle mit bekannter Antwort: identische Bilder (auch 1×1, 1×9, 9×1, 2×2): Fehler 0, SSIM 1; zwei flache Grautöne:
     Fehler = Helligkeitsunterschied, SSIM = `(2·L1·L2 + C1)/(L1² + L2² + C1)`; Schachbrett ±0,06 um das Grau:
     mittlerer Fehler 0,052, weichgezeichneter 0,0006 (der Weichzeichner löscht ein Schachbrett fast vollständig);
     Vertauschen der Bilder ändert nichts; mehr Rauschen: SSIM fällt und weichgezeichneter Fehler steigt bei jeder
     Stufe; ungleiche Größen und leere Bilder: `valid == false`.
  3. Pipeline: standardmäßig aus; eingeschaltet ändert sich weder Frame noch Fehler; `metrics.mean_error` ==
     `mean_oklab_error` (1e-9); Floyd-Steinberg gegen „none“: pro Pixel gewinnt „none“, weichgezeichnet gewinnt der
     Dither; `convert()` liefert dieselben Zahlen; `render_sequence`: drei Frames, keiner mit Metriken.
- Der Test hat einen Fehler in meiner ersten Fassung gefunden: Die Zwischenebenen waren `float`. Die SSIM nimmt
  Varianzen als E[x²] − E[x]²; in `float` ist diese Differenz um etwa 3e-8 verrauscht, gegen C2 = 9e-4 ist das bei
  flachen Bildbereichen (Varianz 0) schon ein Fehler von 3e-5 in der SSIM. Jetzt rechnet alles in `double`.
- Gegenprobe an einem echten Ergebnis (8-Bit-PNGs der Leinwand und der Vorschau, NumPy): Fehler 0,012584 (Programm
  0,012559), weichgezeichnet 0,001333 (0,001313), SSIM 0,90452 (0,90465). Die kleine Abweichung kommt von der
  8-Bit-Rundung der PNGs, das Programm rechnet auf der Float-Leinwand.

### Symbol-Buttons mit Tooltips

Wo ein Symbol sicherer und platzsparender ist als ein Wort, ersetzt es jetzt den Text; jeder dieser Knöpfe hat
einen Mouse-over-Text (deutsch und englisch). Die Symbole sind wie die Dreh-/Spiegel-Symbole 24×24-Alphamasken
(`scripts/make_gui_icons.py` erzeugt `tools/renderhamgui/GuiIcons.hpp` und eine Vorschau in
`testdata/out/gui_icons_preview.png`), zur Laufzeit in der Textfarbe des Themes eingefärbt, also auch auf
dunklen Themes lesbar.

| Wo | vorher | jetzt (Tooltip) |
|---|---|---|
| Zoom-Kasten (Quelle und Vorschau, je 4) | „100%“, „Fit“, „-“, „+“ | 1:1, Rahmenecken, Lupe −, Lupe + („Mit 100 % anzeigen“, „Das ganze Bild ins Fenster einpassen“, „Verkleinern“, „Vergrößern“) |
| Zielfarbkorrektur | „Reset“ | Rückgängig-Pfeil („Helligkeit, Kontrast, Gamma und Sättigung auf ihre neutralen Werte zurücksetzen“) |
| Keyframes | „Add keyframe“, „Remove“ | Keyframe-Raute mit Plus / Minus (Tooltips wie bisher, plus Kürzel Strg+K); die Auswahlliste bekommt die frei gewordene Breite |
| Vorschau-Unterbox | Kontrollkästchen „Loop“, „Back and forth“ | Umschalt-Knöpfe mit Wiederholen-Symbol und Doppelpfeil; gedrückt = an |
| „Show HAM preview“ / „Play preview“ / „Stop preview“ | nur Text | Text **mit** ▶ / ■ je nach Zustand |
| „Cancel“ neben dem Fortschrittsbalken | nur Text | Text mit ✕ |

Bewusst **nicht** umgestellt: „Open…“, „Save“, „Save As…“, „Target directory…“ (der Text von „Save“ wechselt zu „Export
Animation“, ein Symbol würde das verschleiern), „Load/Save palette…“, „Settings…“, „About“ (selten benutzt, der Text
ist auffindbarer) und „Lock aspect ratio“ (Kontrollkästchen mit Text neben der Seitenverhältnis-Auswahl, ein
Vorhängeschloss allein wäre dort mehrdeutiger).

**Dabei behobener Fehler:** Symbol-Knöpfe sahen im deaktivierten Zustand genauso aus wie aktive (wxWidgets/GTK graut ein
Bitmap nicht von selbst). Das betraf auch die Dreh-/Spiegel-Knöpfe vor dem Laden eines Bildes. Jetzt bekommt jedes Symbol
ein zweites Bitmap in der grauen Textfarbe des Themes (`SetMaskIcon` in `IconUtil.hpp`).

**Verifikation (Xvfb, Screenshots, 1920×1080).** Alle Symbole in der Oberfläche; Fensterhöhe unverändert (898×971, die
Parameterfläche scrollt wie vorher); ohne Bild die Dreh-Knöpfe sowie ohne aktive Animation Keyframe-, Loop- und
Hin-und-her-Knöpfe ausgegraut; bei aktiver Animation sind diese Knöpfe aktiv, ein angeklickter Umschalt-Knopf zeigt
„gedrückt“, und „Play preview“ samt ▶ bleibt mit nur einem Keyframe ausgegraut. Tooltips per Mausbewegung ausgelöst und abfotografiert: Zoom „Fit“, Loop, Statuszeile.
Deutsche Oberfläche: „Rendering“, „Lookahead: aus“, Statuszeile passt in die Breite (zuerst wurde sie abgeschnitten,
„weichgez.“ ist zu „weich“ geworden). ctest: alle sieben Tests grün.

## Bekannte Grenzen / Ausbaustufe 2+

- Nur klassisches Voll-Bild-HAM6 (eine Palette fürs ganze Bild). Sliced-
  HAM / Dynamic-HiRes (Copper-Palettenwechsel pro Scanline für mehr
  Farben bzw. Hires-Auflösung) ist architektonisch vorbereitet
  (`HAM6Frame`/`ilbm_writer` sind bewusst getrennte Module), aber noch
  nicht implementiert.
- Farbfokus wirkt bislang nur auf Saat-Palette und
  Cluster-Mittelwerte, nicht auf die Cluster-*Anzahl* pro Region (siehe
  oben) – spürbar, aber mit Ausbaupotenzial.
- GUI (`tools/renderhamgui/`) ist ein erster Ausbau: Konvertierung und
  Export laufen im Hintergrund (mit Abbruch), aber es gibt keine
  automatische, entprellte Live-Vorschau bei jeder Slider-Änderung und
  keinen Batch-Betrieb. Zuschnitt hat inzwischen ein
  bidirektional editierbares Rubber-Band; Drag-and-Drop ist implementiert,
  aber in dieser Sandbox nicht interaktiv testbar (siehe GUI-Abschnitt).
- `--pixel-aspect` ist ein Korrekturfaktor, kein fest hinterlegter
  "korrekter" Amiga-Wert – dazu gibt es zu viele monitor-/
  overscan-abhängige Konventionen, um hier einen falschen Anspruch auf
  Genauigkeit zu erheben.
