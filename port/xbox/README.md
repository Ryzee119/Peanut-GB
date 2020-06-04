## Original Xbox Port

## Building

Setup and install nxdk. Refer to `https://github.com/XboxDev/nxdk`  
We also need a [wip audio fix](https://github.com/XboxDev/nxdk-sdl/pull/27) (until merged into master)
```
cd path/to/nxdk
cd lib/sdl/SDL2
git fetch origin pull/27/head:pr27
git checkout pr27
```

Then do this from your home directoy:  
```
cd~
export NXDK_DIR=/path/to/nxdk
git clone https://github.com/Ryzee119/Peanut-GB.git
cd Peanut-GB/port/xbox/
make
```
Compiled xbe can be found in `Peanut-GB/port/xbox/bin/`

It is currently hard coded to look for `rom.gb` within the same directory as the `default.xbe`.

Save files will be saved to `E:\rom.sav`

Controls are as expected, with some additions:

 - White - Decrease Game Speed
 - Black - Increase Game Speed
 - Y - Toggle Frame Skip on or Off
 - BACK+START - Reset Gameboy
 - BACK+Y - Save screenshot to E:\
