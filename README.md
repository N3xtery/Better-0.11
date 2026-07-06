<img src="https://github.com/user-attachments/assets/20ea0dee-55dc-440e-a49c-d0d2eff3786d"/>

# Better 0.11
A Blocklauncher addon (native mod) making 0.11.1 a better version!

## What's done?
- Replaced dirt paths in villages with gravel. The way it's supposed to be!
  - To make sure you don't accidentally ruin your grass blocks, the ability to make dirt paths is removed too!
- Removed skin packs. No microtransactions in my game please!
- 0.11 added boats, but they're so slow! This addon makes them just as fast as on PC, 2.25 times faster to be exact!
- Huh? Rotten flesh added before hunger? Fixed, zombies don't drop it anymore!
  - Even if you somehow get your hands on some, you won't be able to eat it!
- Raw chicken can give you the hunger effect? While there's no hunger bar yet, really? Fixed!
- Baby squids? Who even came up with that? Removed their ability to spawn!
- Andesite, diorite and granite? Inventory clutter in my nostalgic old Minecraft? Nope, you no longer generate!
  - Removed their crafting recipes as well!
- 0.11 came with an unpleasant bug where players turn dark when being in a non-full block directly under a full one ([MCPE-8392](https://bugs.mojang.com/browse/MCPE/issues/MCPE-8392)). Fixed!
- Old MCPE has a known bug where when you quit in a 2-block high area and rejoin, you get teleported all the way up ([MCPE-7037](https://bugs.mojang.com/browse/MCPE/issues/MCPE-7037)). So annoying, fixed!
- Removed the item descriptions from furnaces? How could you? They're back now!
  - Apparently you couldn't smelt Coal, Lapis and Redstone ores, no problem with Diamond and Emerald ores though. You can smelt them all now, because why not!
- 0.11 also made mushroom islands break the game when you come near them ([MCPE-8826](https://bugs.mojang.com/browse/MCPE/issues/MCPE-8826)). Don't worry no more, as this addon saves the Mooshrooms!

## Installation
1. Have MCPE 0.11.1 installed. You can download it from Minecraft Wiki (choose the reupload version).
2. Have Blocklauncher Pro installed. Version 1.9.12 is the latest one for 0.11.1. The patched variant for new Android versions is available [here](http://www.nostalgiaforum.rf.gd/index.php?threads/156/).
3. [Download](https://github.com/N3xtery/Better-0.11/releases/latest/) the addon's APK and install it.
4. ???
5. Profit!

## Building
I used Android NDK r10e, Android build tools r22.0.1, Android tools r25.2.5, Android platform tools r19.0.2 and Android platform 19 r04. Crazy how you need all that for a single APK! Then you need 0.11.1's libminecraftpe.so in the JNI folder, together with libmcpelauncher_tinysubstrate.so. In the project root folder you run the `ndk-build` command, followed by `ant debug`. Your APK is now in the bin folder!
