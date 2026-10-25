# Build — Spawn The World v1.0.0

## Voraussetzungen

- Windows x64
- LLVM/Clang für Windows mit `clang-cl.exe`, `clang.exe` und `lld-link.exe` in `PATH`
- Python 3 als `python.exe` in `PATH`

Für diesen Minimal-Build sind kein vollständiges Visual-Studio-Projekt und keine zusätzliche Mod-DLL erforderlich. Die benötigten Import-Libraries werden aus den beigefügten `.def`-Dateien erzeugt.

## Bauen

In einer Windows-Eingabeaufforderung im Projektordner ausführen:

```bat
BUILD-WINDOWS.cmd
```

Das Skript erzeugt:

```text
dist\dinput8.dll
```

Temporäre Objektdateien und Import-Libraries landen in `build\`.

Danach kann die erzeugte DLL zusätzlich geprüft werden:

```bat
python scripts\check_build.py dist\dinput8.dll
```

## Build-Einstellungen

- Die normalen C++-Dateien werden mit `clang-cl /O2 /GS /GR- /Zl` gebaut.
- `SecurityCookie.cpp` wird absichtlich mit `/GS-` kompiliert, weil dort der Security-Cookie selbst initialisiert wird.
- `lld-link` erstellt eine Windows-x64-DLL ohne Standard-CRT.
- `scripts/make_version_resource.py` erzeugt die Windows-`VERSIONINFO`-Ressource für `dinput8.dll`.

## Reproduzierbarkeit und SHA-256

Der Quellcode lässt sich zu einer funktionsgleichen Windows-x64-DLL kompilieren. Eine neu kompilierte Datei muss jedoch **nicht byteidentisch** mit der veröffentlichten und getesteten DLL sein.

Compiler-Version, Linker-Version und andere Build-Details können den binären SHA-256 verändern, obwohl der Quellcode gleich ist.

Der SHA-256 in `REFERENCE-SHA256.txt` gehört deshalb ausdrücklich zur **konkret getesteten Release-Binary** und ist kein versprochener Hash für jeden späteren Rebuild.

## Technische Grundlage

Version 1.0.0 verwendet die im Spiel bestätigte asynchrone Hook-Architektur. Die Spawn-Logik, der Gateway-Code und die eingebettete Gegnerliste entsprechen der getesteten Release-Fassung.

Der Hook ändert beim Einbau kurzzeitig die Speicherrechte der betroffenen Engine-Code-Seite. Nach Spielupdates kann die interne Hook-Position ungültig werden; in diesem Fall verweigert die Mod den Hook oder benötigt ein Update.

`scripts/check_build.py` führt statische Prüfungen der erzeugten DLL durch. Es ersetzt keinen Laufzeittest in Enshrouded.
