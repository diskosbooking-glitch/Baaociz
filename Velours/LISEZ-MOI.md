# Velours v0.3.0 — Aociz

Suppresseur de résonances dynamique AU + VST3 (macOS), inspiré de soothe3
(oeksound). Il repère en continu les fréquences qui « sonnent » trop (dureté
d'une voix, sifflantes, résonances d'une pièce, notes de basse qui ressortent)
et les atténue uniquement quand elles apparaissent.

![Aperçu](docs/apercu.png)

## Les réglages

| Réglage | Rôle |
|---|---|
| **DEPTH** | Quantité de réduction (0 à 20). 5 = discret, 10 = net, 20 = très fort |
| **DETAIL** | Bas = traitement large et doux ; haut = coupes étroites et chirurgicales, uniquement sur les pics marqués (remplace Sharpness + Selectivity de soothe2, comme dans soothe3) |
| **SOFT / HARD** | Soft : seuil adaptatif, indépendant du niveau, transparent. Hard : seuil fixe, dépend du niveau absolu et réagit aux montées soudaines, plus ferme (proche d'un compresseur multibande) |
| **DETAIL TILT** | Fait varier Detail selon la fréquence (+ = plus de détail dans les aigus) |
| **ATTACK / RELEASE** | Vitesse de réaction et de retour des coupes (naturellement plus rapides dans l'aigu, comme soothe3) |
| **ATTACK TILT / RELEASE TILT** | Font varier l'attaque et le relâchement selon la fréquence (+ = aigus plus rapides, graves plus lents) |
| **MAX CUT** | Plafond d'atténuation, quelle que soit la fréquence |
| **L/R – M/S, LINK** | Traitement gauche/droite ou mid/side ; Link 100 % = même traitement sur les deux canaux |
| **SIDECHAIN** | La détection se fait sur l'entrée sidechain |
| **MIX, WET TRIM, OUTPUT** | Dosage sec/traité (aligné en phase), gain du traité seul, gain de sortie |
| **DELTA** | N'écouter que ce qui est retiré |
| **LISTEN** (barre de bande) / Alt + glisser | N'entendre que ce qui est retiré dans la zone d'une bande |
| **FOCUS** | Déplace le traitement vers L/Mid (à gauche) ou R/Side (à droite) |
| **RESOLUTION** | Low Latency (21 ms), Normal (43 ms), High Res (85 ms, résolution la plus fine) — latence compensée par le DAW |
| **QUALITY** | Résolution temporelle : Normal, High (2x CPU), Ultra (4x CPU), sans changer la latence. **RENDER IN ULTRA** : Ultra automatique à l'export |
| **A / B, UNDO / REDO, SAVE** | Comparaison de deux réglages, annuler/rétablir, presets utilisateur (~/Library/Application Support/Velours/Presets) |

## Éditeur de bandes (écran central)

La courbe jaune règle la **sensibilité** du traitement selon la fréquence (ce
n'est pas un égaliseur). Par défaut : coupe-bas, une cloche, coupe-haut.
Jusqu'à 8 bandes : cloche, shelf grave/aigu, coupe-bas/haut, passe-bande, coupe-bande (Band Reject), tilt.

- **double-clic** dans le vide : ajouter une bande (Cmd + double-clic : passe-bande) ;
- **clic** sur une bande : elle s'édite dans la **barre sous le graphe** (ON, LISTEN,
  forme, fréquence, profondeur, Q, focus, suppression), comme dans soothe3 ;
- **glisser** une bande : fréquence + sensibilité (Maj = fréquence seule) ;
- **molette** : largeur (Q), Maj + molette = réglage fin ;
- **double-clic** sur une bande : la mettre en pause (cercle vide) / la réactiver ;
- **clic droit** : forme, **Focus** stéréo (All / Left / Right / Mid / Side), pause, supprimer.

Les zones grisées ne sont pas traitées. En M/S ou L/R, si des bandes ont un
Focus différent, la courbe du 2e canal apparaît en pointillés.

## Interface (v0.3)

Sobre et lisible, dans l'esprit de soothe3 mais avec l'identité Velours :
gris profond, un seul accent « velours », police Inter embarquée.

- à gauche : **DEPTH** et **DETAIL** (gros potards), **ATTACK / RELEASE**, **SOFT / HARD** ;
- au centre : le **graphe de réduction**. L'aplat rose descend du haut : c'est la
  réduction en cours, fréquence par fréquence (repères -3 à -24 dB). Le trait
  rose pâle garde la trace des coupes récentes. Les points marquent les
  résonances suivies et la liste **TRACKING** donne les 4 plus coupées
  (fréquence, note, dB). Courbe blanche = courbe de profondeur (bandes). Zone
  assombrie sous le pointillé = au-delà de MAX CUT ;
- sous le graphe : la **barre de la bande sélectionnée** ;
- à droite : **TILT** (detail, attack, release), **MAX CUT / WET TRIM**,
  **STEREO** (L/R – M/S, LINK, FOCUS, SIDECHAIN), **RENDER IN ULTRA** ;
- en bas : BYPASS, DELTA, MIX, OUTPUT, réduction moyenne, RESOLUTION, QUALITY ;
- en haut : presets (usine + utilisateur, SAVE), A / B, UNDO / REDO, taille de fenêtre.

## Vérifications avant envoi

Banc d'essai hors DAW (vrais fichiers source, JUCE 8.0.9) :
- depth 0 : reconstruction parfaite (erreur < 5e-7) à 44,1 / 48 / 96 / 192 kHz,
  latence mesurée = latence annoncée dans les 3 résolutions ;
- bruit blanc seul : moins de 1 dB de baisse (rien à corriger) ;
- résonance étroite à 3 kHz : -10 dB à depth 5, -20 à depth 10, -29 à depth 20 (Hard : -15 dB à depth 5) ;
- résonance qui glisse de 1 à 4 kHz : repérée 28 fois sur 28, à 0,02 octave près ;
- BAND LISTEN : on entend le retiré dans la zone de la bande, le reste est 40 dB plus bas ;
- voix synthétique (harmoniques) : la résonance est coupée, les harmoniques
  normales restent intactes (-0,1 à -0,5 dB à Detail 50 et plus) ;
- un état sauvegardé en v0.1 / v0.2 se recharge correctement ;
- QUALITY High / Ultra : même réduction qu'en Normal (-10,1 dB), même latence, reconstruction parfaite ;
- FOCUS +100 % : canal gauche intact, droit traité (-15 dB) ; A/B, annuler et presets utilisateur vérifiés ;
- Max Cut respecté au dixième de dB ; Delta + traité = signal sec exact ;
  bypass et mix 0 % alignés sur la latence ;
- sidechain, mono, M/S, blocs de taille aléatoire, silence, impulsions : aucune
  valeur invalide ;
- pluginval niveau 10 : réussi ; CPU ~2 à 12 % d'un cœur selon la résolution.

## Historique

**v0.3.0** — nouvelle interface et améliorations techniques :
- interface entièrement redessinée (sobre, un accent, police Inter, grand graphe
  de réduction, barre de bande façon soothe3, panneau latéral, pied de page) ;
- QUALITY Normal / High / Ultra (pas de 5,3 / 2,7 / 1,3 ms) sans changer la
  latence, et **Ultra automatique au rendu** (comme soothe3) ;
- FOCUS stéréo global ; bandes en pause (double-clic sur le nœud) ;
- A / B, annuler / rétablir, presets utilisateur ;
- écran plus léger (100 images/s max) même en Ultra.

**v0.2.0** — action nettement plus audible et vrai suivi des résonances :
- coupes presque 2 fois plus profondes à réglage égal (résonance test :
  -10 dB à Depth 5 au lieu de -5,6) sans toucher aux harmoniques normales ;
- détection lissée dans le temps (plus stable, plus lente en Detail élevé,
  comme soothe3) : une résonance qui glisse de 1 à 4 kHz est suivie à 1/50
  d'octave près ;
- Detail bas : enveloppe de 4 octaves pour traiter les accumulations larges ;
- temps d'attaque et de relâchement naturellement plus courts dans l'aigu,
  ATTACK TILT et RELEASE TILT séparés ;
- mode HARD dépendant du niveau (seuil fixe), plus ferme ;
- écran façon soothe3 : graphe de réduction plein écran, traînées, repères sur
  chaque résonance suivie, liste TRACKING, zone MAX CUT grisée ;
- BAND LISTEN (bouton ou Alt + glisser) ; nouvelle forme de bande Band Reject.

**v0.1.0** — première version : moteur STFT, Depth / Detail / Soft-Hard /
Attack / Release / Max Cut / Tilt (détail et temps), éditeur de 8 bandes avec
Focus stéréo, L/R – M/S + Link, sidechain, Delta, Mix, Wet Trim, 3 résolutions,
16 presets.
