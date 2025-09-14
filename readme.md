# Wii Homebrew Chainloader

A tiny homebrew forwarder for the Nintendo Wii.  

---

## 🚀 Usage

1. Copy the `hackless.elf` to your SD card (`sd:/private/wii/app/rsbe/st/hackless.elf`).
2. Place your main app in `sd:/apps/<name>/boot.dol`.
3. Rename the modded Smashstack `st_private.bin` to the name of your choice and place it in `sd:/private/wii/app/rsbe/st/`. *Make sure there is only one bin file in there*
4. Run Smash Stack
   - The forwarder finds `<name>` from the `st_*.bin` filename.  
   - Loads `sd:/apps/<name>/boot.dol`.