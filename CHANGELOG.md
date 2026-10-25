# Changelog

## 1.0.0 — 25.10.2026

- Erste öffentliche Version von **Spawn The World**.
- F12-Direktspawn auf Basis der stabilen asynchronen Hook-Architektur.
- Single-DLL-Aufbau als `dinput8.dll`.
- 865 Gegner-GUIDs direkt in der DLL eingebettet.
- Windows-Versionsinformationen eingebettet.
- Compiler-Stackschutz `/GS` mit explizit initialisiertem Security-Cookie.
- Gateway und Trampolin nach dem Aufbau mit restriktiveren Speicherrechten; veränderliche Steuerdaten bleiben separat beschreibbar.
- Im Spiel erfolgreich getestet.
