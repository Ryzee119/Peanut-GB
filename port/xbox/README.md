## Original Xbox Port

## Building

Setup and install nxdk. Refer to `https://github.com/XboxDev/nxdk`  
Then do this:  
```
cd Peanut-GB/port/xbox/
make NXDK_DIR=/path/to/nxdk
```

It is currently hard coded to look for Roms within `Peanut-GB/Roms/`.

Save files will be saved to `Peanut-GB/Saves/*.sav`

Controls are as expected, with some additions:

 - White - Decrease Game Speed
 - Black - Increase Game Speed
 - Y - Toggle Frame Skip on or Off
 - BACK+START - Exit to Main Menu
 - BACK+Y - Save screenshot to E:\
 - BACK+DUP/DDOWN - Cycle through color palettes
 - BACK+DLEFT - Assign auto color palette is available
