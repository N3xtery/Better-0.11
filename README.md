<img src="https://github.com/user-attachments/assets/20ea0dee-55dc-440e-a49c-d0d2eff3786d"/>

# Better 0.11
A Blocklauncher addon (native mod) making 0.11.1 a more consistent and stable version

## Features
- Replaced dirt paths in villages with gravel
  - The ability to make dirt paths is removed as well
- Removed skin packs
- 0.11's boats are very slow, so their speed is increased 2.25 times to match newer boats
- Zombies no longer drop rotten flesh
  - Existing rotten flesh will be inedible
- Raw chicken won't give you the hunger effect anymore
- Removed the ability of baby squids to spawn
- Andesite, diorite and granite no longer generate
  - Their crafting recipes are removed too
- 0.11 came with an unpleasant bug where players turn dark when being in a non-full block directly under a full one ([MCPE-8392](https://bugs.mojang.com/browse/MCPE/issues/MCPE-8392)), fixed
- Fixed a known bug of old MCPE where when you quit in a 2-block high area and rejoin, you get teleported all the way up ([MCPE-7037](https://bugs.mojang.com/browse/MCPE/issues/MCPE-7037))
- Brought back item descriptions in furnaces
  - Made Coal, Lapis and Redstone ores smeltable, since Diamond and Emerald ones already were
- 0.11 also made mushroom islands break the game when you get near them ([MCPE-8826](https://bugs.mojang.com/browse/MCPE/issues/MCPE-8826)), fixed
- All loaded chunks of non-flat worlds get saved now, preventing some terrain features getting cut off
- Baby zombies can't spawn anymore
- Fixed the stutter of minecarts and boats on chunk borders
- Fixed occurances of the player immediately getting up after using a bed
- Solved the stuttering of minecarts, boats and their riders in multiplayer
- Fixed the player's animation being slow when flying
- All the features of the addon are optional and can be disabled in the config file located at /games/com.mojang/minecraftpe/better011.txt, where you can also set the port and the maximum amount of players for your local server multiplayer world

## Installation
1. Have MCPE 0.11.1 installed (the original (-1) should work but the reupload (-2) is recommended). You can download it from the English Minecraft Wiki.
2. Have Blocklauncher Pro installed. Version 1.9.12 is the latest one for 0.11.1. The patched variant for new Android versions is available [here](http://www.nostalgiaforum.rf.gd/index.php?threads/156/).
3. [Download](https://github.com/N3xtery/Better-0.11/releases/latest/) the addon's APK and install it.

## Building
I used Android NDK r10e, Android build tools r22.0.1, Android tools r25.2.5, Android platform tools r19.0.2 and Android platform 19 r04. So much for a single APK... Then you need 0.11.1's libminecraftpe.so in the JNI folder, together with libmcpelauncher_tinysubstrate.so. In the project root folder you run the `ndk-build` command, followed by `ant debug`. Your APK is now in the bin folder
