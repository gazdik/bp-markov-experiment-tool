----------------------------------------------------------------
- experimentTool - Nástroj pre vyhodnotenie úspešnosti pri generovaní hesiel s
využitím Markovského modelu
- Autor: Peter Gazdík
- Licencia: MIT
----------------------------------------------------------------

Pomocou tohoto nástroja boli vykonané všetky merania popísané v priloženej
dokumentácii.

################################################################
# Preklad
################################################################

Nástroj je možné preložiť s využítím CMake (verzia 2.8 a vyššia).
Pod Windowsom je potrebný preklad pod cez Visual Studio, nakoľko
MINGW nepodporoval v čase písania tejto nápovedy vlákna v C++11.
Okrem toho sú vyžadované OpenCL ovládače vo verzií 1.2.

Postup prekladu:

Linux:

cmake -G"Unix Makefiles"
make

Windows:

cmake -G"Visual Studio 2013 Win64"
make

Spustiteľný súbor sa nachádza po preklade v zložke bin/

################################################################
# Použitie
################################################################

Parametre viď nápovedu aplikácie (parameter -h). Použitie sa veľmi nelíši
od nástroja Wrathion.

Príklad použitia:

./experimentTool -d dictionaries/rockyou_1-7.dic -s stats/rockyou.wstat
-t 25 -M classic -l 1:7
