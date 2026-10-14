# 🍌 DOOM for Banana OS (Bêta)

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
- **Contrôles Clavier AZERTY & QWERTY** :
  - <kbd>Z</kbd> / <kbd>W</kbd> : Avancer
  - <kbd>S</kbd> : Reculer
  - <kbd>Q</kbd> / <kbd>A</kbd> : Tourner à gauche
  - <kbd>D</kbd> : Tourner à droite
  - Touches maintenues continues grâce à un filtre temporel typematic logiciel.
- **Prise en charge de la Souris** :
  - **Rotation libre / Visée fluide** via les deltas de mouvement de la souris.
  - **Clic gauche** : Tirer
  - **Clic droit** : Action / Ouvrir les portes
  - **Molette de souris** : Changement d'arme précédent / suivant
- **Neutralisation du Curseur** : le curseur flèche du système est masqué pendant le jeu (option `-showcursor` pour le forcer si nécessaire).
- **Architecture Universelle** : paquet binaire `.bpk` contenant à la fois les exécutables 32-bit (`app-i686`) et 64-bit (`app-x86_64`).

---

## 🐛 Bugs Connus et Limitations (Bêta)

1. **Capture de souris dans les hyperviseurs** :  
   - Si vous utilisez **VirtualBox**, l'intégration automatique de la souris peut afficher un double pointeur Windows ou limiter les mouvements brusques.  
   - **Solution recommandée** : Désactiver l'intégration de la souris avec <kbd>Ctrl Droite</kbd> + <kbd>I</kbd> afin que VirtualBox capture la souris à 100%.
2. **Pas d'accélération matérielle 3D / GPU** :  
   - Banana OS s'exécute sur le framebuffer logiciel du noyau (`fb`). Le framerate dépend des performances CPU de la machine virtuelle.
3. **Audio / Musique** :  
   - Le moteur audio et la musique MIDI ne sont pas encore synchronisés avec le pilote DMA audio du noyau Banana OS dans cette bêta.
4. **Comportement du `wget` dans Banana OS** :  
   - Sur Banana OS, retélécharger un fichier sans supprimer l'ancien crée `fichier.bpk.1` au lieu d'écraser. Pensez toujours à exécuter `rm -f doom.bpk*` avant un `wget`.

---

## 📥 Installation Rapide sur Banana OS

Ouvrez un terminal dans Banana OS (`startx` si vous êtes en mode texte) :

```bash
# 1. Nettoyer les éventuels anciens téléchargements
rm -f doom.bpk*

# 2. Télécharger le paquet
wget http://<IP_HOTE>:8000/doom.bpk

# 3. Installer le paquet
pkg install -y doom.bpk

# 4. Lancer DOOM
doom -fullscreen
```

### Options de ligne de commande

```text
doom [-fullscreen | -fs] [-ms <1-50>] [-showcursor] [mon_wad.wad]
```
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
