# Skygrin v0.4.0 — Aociz

Effet de build-up AU + VST3 (macOS). Un seul potard **Intensity**.

## Historique des corrections

**v0.1** — le potard pilotait la quantite d'effet, rien ne montait. Le niveau
baissait meme quand on montait le potard. Le frequency shifter etait sur le
signal direct et rendait tout metallique.

**v0.2** — ajout d'un Shepard tone. Correctement realise (barycentre spectral
pondere en sonie stable a 0,03 octave pres, mesure faite) mais mauvais concept :
un Shepard tone tourne indefiniment sans jamais arriver nulle part, alors qu'un
build-up doit partir d'en bas et atterrir en haut.

**v0.3** — la hauteur est pilotee par la position du potard. Riser harmonique de
110 Hz a 2 490 Hz sur 4,5 octaves. Montee verifiee monotone.

**v0.4** — le riser harmonique avait une tonalite propre (cale sur un La) qui se
disputait avec celle du morceau. Remplace par un riser de bruit.

## Le riser de bruit

Deux bandes de bruit tres resonantes suivent la trajectoire du potard (au double
et au triple de la fondamentale), plus un lit de bruit large qui s'ouvre
progressivement.

A facteur de qualite eleve, une bande de bruit laisse percevoir une hauteur qui
monte tout en restant soufflee : elle ne peut donc pas entrer en conflit avec la
tonalite du morceau.

La resonance maximale depend du preset — c'est le parametre `RES` dans
`Presets.h`, avec Q = 2 + valeur x 26 :

| Preset          | RES  | Q max | caractere                       |
|-----------------|------|-------|----------------------------------|
| Warm Up         | 0,30 | 9,8   | souffle, discret                 |
| Tunnel Vision   | 0,30 | 9,8   | souffle, discret                 |
| Barber Pole     | 0,45 | 13,7  | intermediaire                    |
| Fist Pump       | 0,55 | 16,3  | hauteur perceptible mais floue   |
| Balloon Head    | 0,55 | 16,3  | hauteur perceptible mais floue   |
| Jaw Drop        | 0,75 | 21,5  | marque                           |
| Rocket Fuel     | 1,00 | 28,0  | quasi tonal, ca siffle           |
| Meltdown        | 1,00 | 28,0  | quasi tonal, ca siffle           |

Le riser sinusoidal de la v0.3 est conserve dans le code mais a zero dans tous
les presets (colonne RISER). Remonter cette valeur pour s'en servir sur un son
isole plutot que sur un master.

## Chaine complete

passe-haut resonant -> passe-bas -> barber pole filter -> + riser de bruit
-> delay (frequency shifter dans la boucle de feedback, temps qui raccourcit
de 1/4 vers 1/16) -> saturation -> gate synchro qui s'accelere -> reverbe
-> plafond doux

**Barber pole filter** : six passe-bande dont les frequences centrales balayent
85 % de leur etendue au fil du potard, sans jamais boucler pendant une montee.
Ce sont les frequences du morceau que l'on entend monter. Le banc perd 13 dB
par rapport a l'entree (mesure sur bruit blanc), compense par un gain de 2,8.

## Mise a jour

Changer `project(Skygrin VERSION x.y.z)` dans `CMakeLists.txt`, renvoyer le
dossier Skygrin sur GitHub, recuperer le .pkg dans Actions -> Artifacts.
