# Skygrin v0.1.0 — Aociz

Plugin d'effet AU + VST3 (macOS) : un seul potard **Intensity** qui pilote toute
une chaîne de traitement pour fabriquer des montées / build-ups.

## Chaîne interne (dans l'ordre)

1. Générateur de bruit filtré (bandpass résonant qui balaye vers l'aigu) → le « riser »
2. Saturation (waveshaping tanh)
3. Bitcrush / décimation (uniquement sur les presets durs)
4. Passe-haut (le grave disparaît)
5. Passe-bas (l'aigu se referme)
6. Frequency shifter SSB (« barber pole ») — décale toutes les fréquences du même
   nombre de Hz, ce qui casse l'harmonicité et donne la sensation de montée infinie
7. Delay calé sur la croche du tempo de l'hôte, avec feedback
8. Phaser
9. Réverbe
10. Plafond doux en sortie

Chaque module a, **par preset**, une valeur max et une courbe (exposant). C'est là
que se joue le caractère : le DSP est classique, le mapping fait tout.

## Presets

Warm Up · Fist Pump · Balloon Head · Tunnel Vision · Barber Pole · Rocket Fuel ·
Jaw Drop · Meltdown

## Mise à jour

Changer le numéro dans `project(Skygrin VERSION x.y.z)` dans `CMakeLists.txt`,
renvoyer les fichiers modifiés sur GitHub, récupérer le `.pkg` dans Actions → Artifacts.
