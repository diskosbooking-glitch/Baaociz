# Subshaper — v0.3.0

Plugin de traitement du grave (AU / VST3), look synthé modulaire pastel.

![Aperçu](docs/apercu-atl-knock.png)

## Architecture
Le grave (sous la fréquence CROSSOVER) passe dans une chaîne de modules cumulables :

GEN → TONE → DRIVE → SHAPE → PUMP, puis WIDTH sur la partie aiguë.

| Module | Rôle | Réglages |
|---|---|---|
| GEN   | Crée un sub (Octave dessous ou Sine) | Level, Tone/Replace, Key Lock |
| TONE  | Résonance accordée sur la note KEY | Amount, Q, Harmonic (1x à 4x) |
| DRIVE | Saturation (Tape, Tube, Hard, Fold) | Drive, Color, Focus (note KEY), Mix |
| SHAPE | Attaque / queue du grave | Attack, Sustain |
| PUMP  | Ducking calé sur le tempo | Rate, Depth, Release |
| WIDTH | Largeur stéréo au-dessus de la coupure | Width |

KEY : potard cranté sur les 12 notes + octave (OCT 1 = zone 808).
TUNER : affiche la note jouée par la basse et l'écart en cents.
MAIN : Input, Crossover, Mix, Output, Mono Low, Solo Low, Sub Cut 25,
Delta (écoute de la différence), Gain Match, HQ (x4 / x2).
Barre du haut : presets, Save, comparaison A/B, Undo/Redo, taille de fenêtre.

## Presets
Trap : ATL Knock, Crushed 808, Drill Glide, R&B Clean Sub, Phone Speaker Rescue
House : Deep House Sine, Tech House Roll, Bass House Growl, Garage Wobble Sub
Utility : Init, Mono Fix, Sub Tighten
Les presets ne changent jamais la note KEY.
Presets utilisateur : ~/Library/Application Support/Subshaper/Presets

## Mise à jour
1. GitHub → dossier **AocizSubShaper** → **Add file → Upload files**.
2. Glisse Source, ci, docs, CMakeLists.txt, build.sh, LISEZMOI.md → **Commit changes**.
3. **Actions** → coche verte → **Artifacts** → **Subshaper-Mac**.

## Installation (Terminal)
    sudo installer -pkg ~/Downloads/Subshaper-0.3.0.pkg -target /

Puis Ableton → Réglages → Plug-ins → Rescan ;
FL Studio → Options → Manage plugins → Find more plugins.
