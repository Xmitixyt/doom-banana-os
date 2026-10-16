# 🍌 DOOM for Banana OS (Bêta)
<img width="648" height="430" alt="image" src="https://github.com/user-attachments/assets/036a0706-1263-4815-99f3-7307027edd92" />

[![Banana OS](https://img.shields.io/badge/OS-Banana%20OS%200.6-yellow.svg)](https://github.com/qcom-toolbox/Banana-OS)
[![Status](https://img.shields.io/badge/status-Bêta%20(expérimental)-orange.svg)]()
[![Arch](https://img.shields.io/badge/arch-x86__64%20%7C%20i686-blue.svg)]()

Portage de **DOOM** (basé sur [doomgeneric](https://github.com/ozkl/doomgeneric)) pour **[Banana OS](https://github.com/qcom-toolbox/Banana-OS)**, un système d'exploitation x86/x86_64 développé from scratch avec environnement de fenêtrage GUI (`bwin`).

> [!WARNING]
> **Statut actuel : Version Bêta (encore un peu buggée)**  
> Ce portage est expérimental et en cours de développement. Certains bugs visuels ou d'entrées peuvent survenir selon l'émulateur utilisé (QEMU, VirtualBox) et la configuration matérielle.

---

## 🎮 Fonctionnalités

- **Trilogie Complète Intégrée (DOOM 1, DOOM 2 et DOOM 3)** :
  - **Prêt à jouer immédiatement** : aucun fichier WAD supplémentaire à chercher ou copier, tout est intégré directement dans le paquet `doom.bpk` !
  - **DOOM 1** (`doom 1`) : version Shareware classique (Épisode 1 : *Knee-Deep in the Dead*).
  - **DOOM 2** (`doom 2`) : le légendaire *DOOM II: Hell on Earth* complet avec ses 32 niveaux, le Super Shotgun et tous les monstres originaux.
  - **DOOM 3 / The Ultimate Doom** (`doom 3` ou `doom`) : la version ultime de DOOM 1 avec l'ensemble des 4 épisodes (*Knee-Deep in the Dead*, *The Shores of Hell*, *Inferno*, et *Thy Flesh Consumed*). WAD réparé et certifié 100% stable (lumps `F_START` / `F_END` restaurés).
- **Capture de Souris Relative Native (SDK API v20 / Build 167+)** :
  - **Rotation 360° fluide et infinie** : fini la souris qui bloque sur les bords de la fenêtre ou de l'écran !
  - **Support matériel VMMouse / VirtualBox** : suspension automatique de l'intégration absolue par le noyau lors de la capture.
  - **Curseur masqué proprement** et gestion des événements relatifs non-bornés (`BANANA_EV_MOUSE_REL`).
  - **Re-capture au clic** : cliquez simplement dans la fenêtre pour reprendre le contrôle après avoir appuyé sur Échap ou changé de focus.
- **Fenêtrage natif Banana OS (Ring 3 / SDK Build 165+)** : rendu graphique optimisé via l'API `bwin` avec exécution sécurisée en espace utilisateur protégé.
- **Mode Plein Écran (`-fullscreen` / `-fs`)** : s'adapte automatiquement à la résolution de l'écran avec bordures noires (pillarboxing) pour préserver le ratio 16:10 d'origine de DOOM.
- **Prise en charge Multi-Touches native (SDK API v14 / build 139)** :
  - Déplacements combinés simultanés (avancer + strafer en diagonale avec <kbd>Z</kbd>+<kbd>D</kbd> ou <kbd>W</kbd>+<kbd>D</kbd>).
  - Sprint / course fluide (<kbd>Shift</kbd> maintenu pendant les déplacements).
  - Tirer ou ouvrir les portes tout en continuant d'avancer sans interruption.
  - Rétrocompatibilité automatique avec filtre typematic logiciel pour les versions antérieures de Banana OS.
- **Contrôles Clavier AZERTY & QWERTY** :
  - <kbd>Z</kbd> / <kbd>W</kbd> : Avancer
  - <kbd>S</kbd> : Reculer
  - <kbd>Q</kbd> / <kbd>A</kbd> : Tourner à gauche
  - <kbd>D</kbd> : Tourner à droite
  - <kbd>Shift</kbd> : Courir / Speed
  - <kbd>Ctrl</kbd> / <kbd>F</kbd> / <kbd>C</kbd> : Tirer
  - <kbd>Espace</kbd> / <kbd>E</kbd> : Ouvrir les portes / Action
  - <kbd>,</kbd> / <kbd>.</kbd> : Pas chassés (Strafe) gauche / droite
- **Prise en charge Complète de la Souris** :
  - **Visée 360° ultra-fluide** via la capture relative native (`-ms <1-50>` pour ajuster la sensibilité, défaut : 4).
  - **Clic gauche** : Tirer
  - **Clic droit** : Action / Ouvrir les portes
  - **Molette de souris** : Changement d'arme précédent / suivant
- **Moteur Audio Haute Fidélité Multi-Canaux** : bruitages et effets sonores complets (tirs d'armes, monstres, portes, explosions, pickups, etc.) mixés en temps réel sur 16 canaux à 48 000 Hz en 16-bit stéréo avec interpolation linéaire anti-crépitement, spatialisation gauche/droite et gestion continue de la file DMA via `banana_play()`.
- **Architecture Universelle** : paquet binaire `.bpk` contenant à la fois les exécutables 32-bit (`app-i686`) et 64-bit (`app-x86_64`).

---

## 🐛 Bugs Connus et Limitations (Bêta)

1. **Carte son dans la Machine Virtuelle** :  
   - Pour entendre le son dans VirtualBox, assurez-vous que l'audio est activé dans les paramètres de la VM (Contrôleur audio : **Intel HD Audio** ou **ICH AC97**).
   - Dans QEMU, démarrez avec `-device AC97` ou `-device intel-hda -device hda-output`.
2. **Musique de fond (MIDI/MUS)** :  
   - Les effets sonores (SFX) sont 100% actifs et fluides. La musique de fond MIDI/MUS n'a pas encore de synthétiseur logiciel intégré dans cette version.
3. **Capture de souris** :  
   - Avec **Banana OS Build 167+ (API v20)**, la capture relative est native et automatique ! Si vous êtes sur une version plus ancienne de Banana OS, désactiver l'intégration de la souris VirtualBox (<kbd>Ctrl Droite</kbd> + <kbd>I</kbd>) est recommandé pour éviter les limitations de bordure.
4. **Comportement du `wget` dans Banana OS** :  
   - Sur Banana OS, retélécharger un fichier sans supprimer l'ancien crée `fichier.bpk.1` au lieu d'écraser. Pensez toujours à exécuter `rm -f doom.bpk*` avant un `wget`.
---

## 📥 Installation Rapide sur Banana OS

Ouvrez un terminal dans Banana OS (`start` si vous êtes en mode texte) :

```bash
# 1. Nettoyer les éventuels anciens téléchargements
rm -f doom.bpk*

# 2. Télécharger le paquet directement depuis GitHub :
wget -O doom.bpk https://github.com/Xmitixyt/doom-banana-os/releases/download/v1.8-beta/doom.bpk
# OU via le lien direct raw GitHub si la release est en cours d'édition :
# wget -O doom.bpk https://raw.githubusercontent.com/Xmitixyt/doom-banana-os/main/releases/doom.bpk

# 3. Installer le paquet (inclut la trilogie DOOM 1, DOOM 2 et The Ultimate Doom)
pkg install -y doom.bpk

# 4. Lancer le jeu de votre choix :
doom 3 -fullscreen   # Lance The Ultimate Doom (DOOM 1 complet avec les 4 épisodes)
doom 2 -fullscreen   # Lance DOOM II: Hell on Earth (32 niveaux complets)
doom 1 -fullscreen   # Lance DOOM 1 Shareware
doom -fullscreen     # Lance par défaut la version complète (The Ultimate Doom)
```

### Options de ligne de commande

```text
doom [1 | 2 | 3] [-fullscreen | -fs] [-ms <1-50>] [-showcursor] [mon_wad.wad]
```
- `1` / `2` / `3` : Sélectionne directement le jeu (**1** = DOOM 1 Shareware, **2** = DOOM II, **3** = The Ultimate Doom).
- `-fullscreen` ou `-fs` : Mode plein écran.
- `-ms <vitesse>` : Règle la sensibilité de rotation de la souris (défaut : 7).
- `-showcursor` : Force l'affichage du pointeur de la souris.

---

## 🛠️ Compilation à partir des sources

La chaîne de compilation utilise **Clang/LLVM** et les scripts du SDK Banana OS :

```bash
python build_doom.py
```

Le script produit le binaire `doom.bpk` prêt à l'emploi.

---

## 📜 Licence

- Moteur DOOM original © id Software (GNU GPL v2).
- Intégration Banana OS & doomgeneric : code communautaire libre sous licence MIT / GPL.
