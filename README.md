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

- **Fenêtrage natif Banana OS** : rendu direct via l'API graphique `bwin` en Ring 0 / mémoire partagée.
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
- **Prise en charge de la Souris** :
  - **Rotation libre / Visée fluide** via les deltas de mouvement de la souris.
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
3. **Capture de souris dans les hyperviseurs** :  
   - Si vous utilisez **VirtualBox**, l'intégration automatique de la souris peut afficher un double pointeur Windows ou limiter les mouvements brusques.  
   - **Solution recommandée** : Désactiver l'intégration de la souris avec <kbd>Ctrl Droite</kbd> + <kbd>I</kbd> afin que VirtualBox capture la souris à 100%.
4. **Comportement du `wget` dans Banana OS** :  
   - Sur Banana OS, retélécharger un fichier sans supprimer l'ancien crée `fichier.bpk.1` au lieu d'écraser. Pensez toujours à exécuter `rm -f doom.bpk*` avant un `wget`.
---

## 📥 Installation Rapide sur Banana OS

Ouvrez un terminal dans Banana OS (`start` si vous êtes en mode texte) :

```bash
# 1. Nettoyer les éventuels anciens téléchargements
rm -f doom.bpk*

# 2. Télécharger le paquet
wget http://<IP_HOTE>:8000/doom.bpk

# 3. Installer le paquet (inclut la trilogie DOOM 1, DOOM 2 et The Ultimate Doom)
pkg install -y doom.bpk

# 4. Lancer le jeu de votre choix :
doom 3 -fullscreen   # Lance The Ultimate Doom (DOOM 1 complet avec tous les épisodes 1, 2, 3, 4)
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
