# Skygrin v0.3.0 — Aociz

Effet de build-up AU + VST3 (macOS). Un seul potard **Intensity**.

## L'erreur des versions precedentes

En v0.1 puis v0.2, le potard pilotait la QUANTITE d'effet pendant qu'un LFO
libre pilotait la HAUTEUR. Les deux n'etaient pas correles : tourner le potard
ne faisait donc monter quoi que ce soit, et la hauteur rebouclait toute seule
(« ca redescend puis ca remonte »).

La v0.2 utilisait en plus un Shepard tone. Mesure faite, il etait correctement
realise — barycentre spectral pondere en sonie stable a 0,03 octave pres — mais
c'est le concept qui ne convient pas ici : un Shepard tone est fait pour tourner
indefiniment sans jamais arriver nulle part. Sur un build-up de huit mesures, on
veut l'inverse : partir d'en bas, arriver en haut, atterrir sur le drop.

## v0.3 : tout est pilote par la position du potard

**Riser** — cinq partiels harmoniques (1, 2, 3, 4, 5), donc une seule hauteur
franche. La fondamentale vaut 110 Hz a 0 % et 2 490 Hz a 100 %, soit 4,5 octaves.
Verification faite sur une montee de huit mesures a 128 BPM : 0 fenetre d'analyse
sur 173 ou la hauteur redescend.

**Barber pole filter** — six passe-bande dont les frequences centrales balayent
85 % de leur etendue au fil du potard, donc sans jamais boucler pendant une
montee. Ce sont les frequences de ton morceau que l'on entend monter.
Le banc perd 13 dB par rapport a l'entree (mesure sur bruit blanc) ; un gain de
compensation de 2,8 est applique.

**Bruit** — bande resonante calee sur le double de la fondamentale du riser,
facteur de qualite de 1,5 a 7,5.

**Delay** — le frequency shifter est dans la boucle de feedback uniquement :
chaque repetition remonte de quelques hertz, l'echo grimpe en escalier. Le temps
du delay raccourcit de 1/4 vers 1/16.

**Gate** — synchro tempo, passe de 1/8 a 1/32 sur les presets durs.

Le gain de sortie monte avec l'intensite (il baissait en v0.1).

## Chaine complete

passe-haut resonant -> passe-bas -> barber pole filter -> + riser -> + bruit
-> delay (shifter dans le feedback) -> saturation -> gate -> reverbe -> plafond doux

## Presets

Warm Up · Fist Pump · Balloon Head · Tunnel Vision · Barber Pole · Rocket Fuel ·
Jaw Drop · Meltdown

Courbes dans `Source/Presets.h` : amount = valeur a 100 %, exp = forme de la
courbe. Modifiable sans toucher au DSP.

## Mise a jour

Changer `project(Skygrin VERSION x.y.z)` dans `CMakeLists.txt`, renvoyer les
fichiers sur GitHub, recuperer le .pkg dans Actions -> Artifacts.
