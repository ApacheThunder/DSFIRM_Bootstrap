# DSFIRM_Bootstrap
Bootstrap for booting DS Firmware SRL (for DSi/3DS mainly)


This bootstrap program for DS/DSi/3DS that can boot firmware.nds file from SD.
The main purpose this program serves is to allow the currently inserted cart in slot1 to show up in the menu!
Inserted carts can be launched but as of v1.0 all carts currently white screen for unknown reasons.

Further research on how the firmware.nds file expects the cart to be setup is needed for this to fully work.

Following files are provided in release builds:
* NDS file for DS/DS Lite users (if you are feeling redundent...:P )
* CIA for 3DS users.
* title folder for HiyaCFW users on DSi
* DSI file for Unlaunch on DSi users.

Whats NOT included: 
* firmware.nds

You must dump this from your a DS/Lite console yourself and is not provided to avoid angering the Nintendo ninjas.
(dumping DS firmware from DSI/3DS consoles will NOT work. Must use original DS Phat/Lite console for this!)

Use fw2nds to build NDS file out of resulting dump. That tool can be found here:

https://gbatemp.net/threads/release-fw2nds-build-firmware-nds-from-firmware-bin.508831/

If you get a "firmware arm binaries too large" error, please post an issue and info about the specific version/console you obtained the dump from.
I hardcoded some size settings for the binaries so most if not all known firmware.nds arm binaries should fit in ram space I
allocated for them for bootloader to relocate but it's possible some versions might not fit. I can expand them a bit if a version that is too large
is found.
