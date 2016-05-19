# markovExperimentTool

Nástroj pre vyhodnotenie úspešnosti pri generovaní hesiel s
využitím Markovho modelu. Pomocou tohoto nástroja boli vykonané všetky merania popísané v [bakalárskej práci](https://goo.gl/mDPK2M) zaoberajúcej sa využitím heuristík pri obnove hesiel na GPU.

## Licencia

Použitie nástroja upravuje MIT licencia, viď `LICENSE.txt`

## Preklad

Nástroj je možné preložiť s využítím CMake (verzia 2.8 a vyššia).
Pod operačným systémom Windows je potrebný preklad cez Visual Studio, nakoľko
MINGW a podobné obskurnosti nepodporujú v súčasnej dobe C++11 vlákna.
Okrem toho sú vyžadované OpenCL ovládače vo verzií 1.2 a vyššej.

### Postup prekladu:

#### Linux

cmake -G"Unix Makefiles"
make

#### Windows

cmake -G"Visual Studio 2013 Win64"
make

Spustiteľný súbor sa nachádza po preklade v priečinku `bin/`

## Použitie

Parametre viď nápovedu aplikácie (parameter `-h`). Použitie sa veľmi nelíši
od nástroja Wrathion.

#### Príklad použitia:

```
./experimentTool -d dictionaries/rockyou_1-7.dic -s stats/rockyou.wstat
-t 25 -M classic -l 1:7
```
