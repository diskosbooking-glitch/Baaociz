# Skygrin v0.6.0 — Aociz

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

**v0.6** — la chaine complete du cahier des charges « build-up machine » :
- **compresseur** (seuil 0 -> -24 dB, ratio 1:1 -> 6:1, makeup automatique) ;
- **saturation** : `juce::dsp::WaveShaper` (tanh) dans une `ProcessorChain`,
  sur-echantillonnee x2 (moins d'aliasing), melangee par un `DryWetMixer` :
  0 % = parfaitement sec, plus aucun basculement brutal ;
- **compensation du grave perdu** : quand le passe-haut vide le kick et la
  basse, le reste remonte (50 % de l'energie perdue, 6 dB maximum) ;
- **air** : shelf aigu `juce::dsp::IIR::Filter` jusqu'a +9 dB a 4,5 kHz, la
  brillance au sommet de la montee ;
- **pitch shifter** : le morceau lui-meme monte (de +30 cents a une octave
  selon le preset). Aucune note ajoutee : c'est le signal d'entree transpose ;
- **delay vraiment synchro** : paliers de notes exacts (1/4 -> 1/8 -> 1/16,
  jusqu'a 1/32) au lieu d'une division continue qui tombait a cote de la grille ;
- **reverbe** : plus grande, plus longue et plus claire en montant ;
- **courbes de reponse** par module et par preset : puissance, courbe en S,
  exponentielle, avec seuil d'entree (`Source/Mapping.h`) ;
- **4 nouveaux presets** : Helium, Cloud Nine, Goosebumps, Countdown ;
- **bug corrige** : `juce::Reverb` double le niveau sec. La v0.5 lui envoyait
  1,0 : +6 dB d'un coup au moindre mouvement du potard. Le niveau monte
  maintenant progressivement ;
- interface : double-clic sur le potard = retour a 0 %, course plus fine,
  la puce du sous-titre s'affiche correctement.

Verification hors DAW (vrais fichiers source, JUCE 8.0.4, gcc et clang,
12 presets, morceau de test kick/basse/accords/charley a -14 LUFS) :
- 0 % : transparent (ecart max 0,4 dB par tiers d'octave, identique a la v0.5) ;
- le niveau monte a chaque palier 0/25/50/75/100 % sur les 12 presets, de
  +6,6 a +9,9 LU a 100 %, sans jamais depasser 0 dBFS ;
- 100 % : grave sous 120 Hz retire de -15 a -33 dB (Warm Up : -9 dB, voulu),
  aigu au-dessus de 6 kHz +7 a +14 dB ;
- retour a 0 % (le drop) : niveau sec retrouve en moins de 300 ms, sans clic ;
- CPU : environ 3 % d'un coeur ; latence declaree 4 echantillons.

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

## Ce qui monte

Uniquement le morceau lui-meme, rien de tonal n'est ajoute : le pitch shifter
transpose le signal, le barber pole filter fait defiler ses frequences vers le
haut, le passe-haut resonant balaye le grave, et chaque repetition du delay
remonte un peu (frequency shifter dans la boucle).

## Chaine complete

Par echantillon : passe-haut resonant -> passe-bas -> air -> compensation du
grave perdu -> pitch shifter -> barber pole filter -> + souffle -> delay synchro
(frequency shifter dans la boucle) -> gate synchro qui s'accelere.

Par bloc : reverbe -> compresseur + makeup -> saturation tanh x2 -> gain de
sortie -> plafond doux.

## Le potard et les presets

Toutes les 32 echantillons, Intensity passe par la courbe de chaque module
(`Presets.h`), les valeurs sont lissees (20 ms), puis `mapToSettings()`
(`PluginProcessor.cpp`) les convertit en Hz, dB, ms. Les modules ne voient
jamais Intensity directement.

Presets : Warm Up · Fist Pump · Balloon Head · Tunnel Vision · Barber Pole ·
Rocket Fuel · Jaw Drop · Meltdown · **Helium** · **Cloud Nine** ·
**Goosebumps** · **Countdown**

Les 8 premiers gardent leurs courbes de la v0.5 (air, pitch et compresseur en
plus). Les nouveaux sont ajoutes en fin de liste : une session enregistree
rouvre sur le meme preset.

Modifier un preset : une ligne par module, `pw` (puissance), `sc` (courbe en S)
ou `ex` (exponentielle) :

    .reverb = sc (0.80f, 3.0f, 0.30f)   // 80 % a fond, courbe en S, dort jusqu'a 30 %

Un module absent de la liste est coupe.

## Compilation et installation

GitHub Actions (workflow « Build Skygrin (macOS) ») produit l'artefact
**Skygrin-Mac** : `Skygrin-x.y.z.dmg` (a ouvrir, puis glisser Skygrin.component
sur « Components (glisser ici) » et Skygrin.vst3 sur « VST3 (glisser ici) »)
et `Skygrin-x.y.z.pkg` en secours (`sudo installer -pkg ... -target /`).
Chaque build publie aussi une Release « Skygrin x.y.z » (onglet Releases du
depot) : lien de telechargement direct du .dmg, sans passer par Actions.

Pour une nouvelle version : changer `project(Skygrin VERSION x.y.z)` dans
`CMakeLists.txt`, envoyer les fichiers DANS le dossier Skygrin du depot.
