# Spawn The World — Enshrouded Mod

**Version:** 1.0.0  
**Plattform:** Windows x64  
**Mod-Datei:** `dinput8.dll`  
**Autor:** faytleingod84

Mit **F12** können unterstützte Gegner und Kreaturen, deren Spawner aktuell von der Spielwelt verwaltet werden, sofort erneut ausgelöst werden. Dadurch lassen sich Gegner und Bosse direkt wieder spawnen, ohne auf normale Respawn-Zeiten oder andere übliche Bedingungen warten zu müssen.

## Installation

1. Enshrouded vollständig beenden.
2. Die veröffentlichte `dinput8.dll` in den Ordner mit `enshrouded.exe` kopieren.
3. Falls dort bereits eine `dinput8.dll` eines anderen Mods liegt, diese nicht einfach überschreiben. In diesem Fall einen kompatiblen Loader oder eine passende Chainload-Lösung verwenden.
4. Spiel starten.
5. Mit **F12** den Spawn auslösen.

**Deinstallation:** Spiel beenden und die `dinput8.dll` entfernen.

## Bedienung

- **F12:** Startet einen neuen Spawn-Durchlauf.
- Ein Durchlauf verwendet ein kurzes Erfassungsfenster von etwa **750 ms**. Weitere Tastendrücke während dieses Zeitraums können ignoriert werden.
- Die Mod enthält standardmäßig **865 eingebettete Gegner-GUIDs**.
- Die aufwendige GUID- und Transform-Prüfung läuft auf einem Hintergrund-Worker. Der Eingriff in den Spielthread bleibt möglichst kurz.
- Logdatei: `%TEMP%\OneButtonSpawner-native.log`

## Quellcode

Dieses Source-Paket enthält:

- `src/SpawnTheWorld.cpp` — Spawn-Logik, Hook, Worker und Synchronisation
- `src/EmbeddedEnemies.cpp` — 865 eingebettete Gegner-GUIDs
- `src/MonolithExports.cpp` und `src/dinput8.s` — DInput8-Weiterleitung
- `src/SecurityCookie.cpp` — `/GS`-Unterstützung für den CRT-freien Build
- `src/*.def` — Windows-Exporte und Importdefinitionen
- `scripts/make_version_resource.py` — erzeugt die Windows-Versionsressource
- `scripts/check_build.py` — statische Prüfung der erzeugten DLL
- `BUILD-WINDOWS.cmd` — erstellt `dist/dinput8.dll`
- `BUILD.md` — Build-Anleitung
- `CHANGELOG.md` — Versionshistorie
- `REFERENCE-SHA256.txt` — Hash der bestätigten Release-Binary
- `SOURCE-SHA256SUMS.txt` — Prüfsummen der Quelldateien
- `LICENSE` — MIT-Lizenz

Eine vollständige Bauanleitung findest du in [BUILD.md](BUILD.md).

## Technischer Hinweis

Die DLL arbeitet als DInput8-Proxy und installiert einen Inline-Hook im eigenen Spielprozess. Dabei werden unter anderem Windows-Funktionen wie `VirtualProtect`, `VirtualAlloc`, `ReadProcessMemory` und `GetProcAddress` verwendet.

Solche Techniken sind bei nativen Mods üblich, können aber bei einzelnen Sicherheitsprogrammen heuristische Warnungen auslösen. Der vollständige Quellcode und die Build-Anleitung werden deshalb öffentlich bereitgestellt.

Die Mod ist spielversionsabhängig. Updates von Enshrouded können Änderungen an der Hook-Position oder anderen internen Strukturen erforderlich machen.

## Lizenz und Credits

Veröffentlicht unter der **MIT License**. Siehe [LICENSE](LICENSE).

Wenn du Code aus diesem Projekt in einem eigenen Projekt verwendest, gib bitte Credit an **faytleingod84 / Spawn The World**.

Dieses Communityprojekt ist **nicht offiziell mit Keen Games verbunden**. Enshrouded und zugehörige Inhalte sind Eigentum ihrer jeweiligen Rechteinhaber.
