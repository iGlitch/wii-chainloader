# Wii Homebrew Chainloader

A tiny homebrew forwarder for [Brawl Mod Launcher](https://github.com/iGlitch/BrawlModLauncher). Built off of FIX94 Wii forwarder v14.

---

## 🚀 Usage

1. Copy the `hackless.elf` to your SD card (`sd:/private/wii/app/RSBE/hackless.elf`).
2. Place your main app in `sd:/apps/<name>/boot.elf`.
2. Copy the `boot.dol` to your app folder (`sd:/apps/<name>/boot.dol`).
3. Place the modded Smashstack `st_260626_1103.bin` and a txt file `<name>.txt` in `sd:/private/wii/app/RSBE/`.
   - The forwarder finds `<name>` from the .txt filename.  
   - Loads `sd:/apps/<name>/boot.elf`.