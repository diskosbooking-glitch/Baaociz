# Memento — instrument AU (v0.1)

Assembleur de « song starters » façon Upcycle, en **instrument Audio Unit** : tu
pointes un dossier de samples (ta *Banque Sons*), Memento pose un sample par
**rôle** (Drums / Bass / Tonal / Texture / Vox), **cale tout au tempo du projet
hôte** (time-stretch SoundTouch) et les joue **ensemble en boucle synchronisée**.
Reroll ALL / par slot, lock, mute / solo / volume par slot.

> v0.1 = cœur assembler synchronisé. Non encore inclus (versions suivantes) :
> calage à une **tonalité** cible, curseur **Chaos↔Safe**, **export stems**,
> panneau *Adjust*, et le **look Miró / Upcycle** (l'UI est fonctionnelle ici).

---

## 1. Où déposer les fichiers (dépôt Baaociz)

À la **racine** de ton dépôt GitHub `Baaociz`, à côté de `AocizSubShaper/` et
`Skygrin/` :

```
Baaociz/
├─ Memento/                       ← ce dossier
│  ├─ CMakeLists.txt              (le n° de version du plugin vit ici)
│  ├─ ci/build_mac.sh
│  ├─ README.md
│  └─ Source/  (PluginProcessor / PluginEditor / MementoEngine .h/.cpp)
└─ .github/workflows/
   └─ build-memento-mac.yml       ← à côté des workflows Subshaper / Skygrin
```

Sur GitHub : **Add file → Upload files**, glisse le dossier `Memento/` puis le
fichier `build-memento-mac.yml` dans `.github/workflows/`, et commit.

## 2. Compiler (GitHub Actions)

Le commit déclenche le workflow **« Build Memento (macOS) »**. Il compile en
**universel** (Apple Silicon + Intel) et produit un installateur `.pkg`.
Onglet **Actions → dernier run → Artifacts → `Memento-Mac`** : télécharge le zip,
dézippe → `Memento-0.1.0.pkg`.

(JUCE et SoundTouch sont récupérés automatiquement par CMake sur le runner — rien
à installer. SoundTouch est sous licence LGPL, récupéré à la compilation.)

## 3. Installer sur ton Mac

```bash
sudo installer -pkg ~/Downloads/Memento-0.1.0.pkg -target /
```

Ça installe `Memento.component` dans `/Library/Audio/Plug-Ins/Components`.
Ensuite : **rescan des AU** dans ton hôte (Logic les revalide au lancement ;
Ableton : Préférences → Plug-Ins → *Rescan*). Vérif optionnelle en Terminal :

```bash
auval -v aumu Mmto Aciz
```

## 4. Utiliser

1. Nouvelle piste **d'instrument** → charge **Memento** (AU).
2. Clique **« Choisir le dossier… »** et sélectionne ta *Banque Sons*
   (la 1ʳᵉ indexation prend un moment ; les slots se remplissent tout seuls).
3. **Lance la lecture** de ton projet : les slots jouent calés à ton BPM.
4. **Reroll ALL** pour une nouvelle combinaison, **Reroll** par slot, **Lock**
   pour figer un slot, **S/M** solo/mute, curseur = volume.

Le dossier choisi et la config des slots sont **sauvegardés avec le projet**.

## 5. Mettre à jour (versions suivantes)

1. Remplace les fichiers modifiés dans `Memento/` sur GitHub (**Upload files**).
2. Incrémente la version dans **`Memento/CMakeLists.txt`** :
   `project(Memento VERSION 0.2.0 …)`.
3. Commit → **Actions** recompile → récupère le nouveau `.pkg` dans **Artifacts**
   → réinstalle avec `sudo installer -pkg … -target /` → rescan AU.
