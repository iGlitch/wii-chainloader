# Wii Homebrew Chainloader

A tiny homebrew forwarder for the Nintendo Wii.  

---

## 🚀 Usage

1. Copy the `hackless.elf` to your SD card (`sd:/private/wii/app/rsbe/st/hackless.elf`).
2. Place your main app in `sd:/apps/<name>/boot.dol`.
3. Place the modded Smashstack `st_private.bin` and a txt file `<name>.txt` in `sd:/private/wii/app/rsbe/st/`. *Make sure there is only one bin file in there*
4. Run Smash Stack
   - The forwarder finds `<name>` from the .txt filename.  
   - Loads `sd:/apps/<name>/boot.dol`.