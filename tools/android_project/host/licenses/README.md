# Eingecheckte Lizenztexte

`Cult-of-GBA-BIOS-LICENSE.txt` ist der einzige hier abgelegte Lizenztext. Das Repo enthaelt fuer das
Ersatz-BIOS in `src/gba_bios.h` nur den Header-Hinweis `License: MIT` mit Quelle und Commit
(Cult-of-GBA/BIOS `0d9dd3e6283896eab340522d51234799d5c46626`), aber keinen vollstaendigen MIT-Text.

Alle anderen Lizenzdateien unter `assets/licenses/` erzeugt der Gradle-Task `collectLicenseAssets`
in `host/build.gradle` beim Build direkt aus den Dateien unter `src/` und der Repo-Wurzel.
