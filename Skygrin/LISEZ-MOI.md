# Skygrin v0.5.0 — Aociz

Effet de build-up AU + VST3 (macOS). Un seul potard **Intensity**.

## Historique des corrections

**v0.1** — le potard pilotait la quantite d'effet, rien ne montait. Le niveau
baissait meme quand on montait le potard. Le frequency shifter etait sur le
signal direct et rendait tout metallique.

**v0.2** — ajout d'un Shepard tone. Correctement realise mais mauvais concept :
un Shepard tone tourne indefiniment sans jamais arriver nulle part, alors qu'un
build-up doit partir d'en bas et atterrir en haut.

**v0.3** — la hauteur est pilotee par la position du potard. Riser harmonique
(sinusoides) de 110 Hz a 2 490 Hz sur 4,5 octaves.

**v0.4** — riser harmonique remplace par deux bandes de bruit tres resonantes.
Jamais compilee : envoyee a la racine du depot au lieu du dossier Skygrin.

**v0.5** — plus aucune note qui monte. La sinusoide est supprimee du code, et
les bandes resonantes de la v0.4 (Q jusqu'a 28, elles sifflaient comme une note
sur les presets durs) sont remplacees par un souffle sans resonance.
Colonnes RISER et RES retirees de `Presets.h`.

## Le souffle qui monte

Bruit blanc filtre par un passe-haut puis un passe-bas Butterworth (Q = 0,707,
aucune bosse de resonance), places une octave de part et d'autre d'un centre
qui suit le potard : 220 Hz a 0 %, 5 kHz a 100 % (la course validee en v0.3).
Bande de deux octaves : on entend le bruit s'eclaircir, jamais une hauteur.
Le lit de bruit large de la v0.4 (le cote noye) est conserve.

Niveau : colonne NOISE dans `Presets.h` ; gain du souffle 0,27 dans
`PluginProcessor.cpp` (2 dB sous les bandes de la v0.4, pondere K).

Verification hors DAW (vrais fichiers source, JUCE 8.0.4, 8 presets, potard
0 -> 100 %, entree silencieuse) : pic le plus saillant au-dessus de son
voisinage = 33 a 49 dB en v0.3, 9 a 17 dB en v0.4, 4,6 a 6,8 dB en v0.5
(bruit pur : 4,6 a 5 dB). Niveau de sortie sur un signal reel inchange.

## Ce qui monte encore

Uniquement le morceau lui-meme, rien n'est ajoute : le barber pole filter fait
defiler ses frequences vers le haut, le passe-haut resonant balaye le grave,
et chaque repetition du delay remonte un peu (frequency shifter dans la boucle).

## Chaine complete

passe-haut resonant -> passe-bas -> barber pole filter -> + souffle
-> delay (frequency shifter dans la boucle de feedback, temps qui raccourcit
de 1/4 vers 1/16) -> saturation -> gate synchro qui s'accelere -> reverbe
-> plafond doux

## Presets

Warm Up · Fist Pump · Balloon Head · Tunnel Vision · Barber Pole · Rocket Fuel ·
Jaw Drop · Meltdown

Courbes dans `Source/Presets.h` : amount = valeur a 100 %, exp = forme de la
courbe.

## Compilation et installation

GitHub Actions (workflow « Build Skygrin (macOS) ») produit l'artefact
**Skygrin-Mac** : `Skygrin-x.y.z.dmg` (a ouvrir, puis glisser Skygrin.component
sur « Components (glisser ici) » et Skygrin.vst3 sur « VST3 (glisser ici) »)
et `Skygrin-x.y.z.pkg` en secours (`sudo installer -pkg ... -target /`).

Pour une nouvelle version : changer `project(Skygrin VERSION x.y.z)` dans
`CMakeLists.txt`, envoyer les fichiers DANS le dossier Skygrin du depot.
