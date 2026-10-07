<sub>[SkyEmu OMEGA](../README.md) › [Docs](README.md) › Accuracy</sub>

# Accuracy

How SkyEmu's Game Boy Advance core compares with other emulators on games and test ROMs that are known to be
hard to emulate. The results were collected for SkyEmu 1.0; every result links to its screenshot.

## Difficult to emulate games

| Game | SkyEmu 1.0 | NanoBoyAdvance 1.6 | mGBA 0.9.3 | VBA-M 2.1.4 | Notes |
|---|:---:|:---:|:---:|:---:|---|
| AGS Aging Cartridge | [✅](accuracy_screenshots/SkyEmu/AGS-aging.png) | [✅](<accuracy_screenshots/NBA/Screen Shot 2022-07-05 at 9.12.25 PM.png>) | [❌](accuracy_screenshots/mGBA/AGS.png) | [❌](accuracy_screenshots/VBA-M/AGS.png) | mGBA and VBA-M do not pass all tests |
| Classic NES Series: The Legend of Zelda | [✅](accuracy_screenshots/SkyEmu/Classic-nes.png) | [✅](accuracy_screenshots/NBA/NES-classics.png) | [✅](<accuracy_screenshots/mGBA/Classic NES Series - The Legend of Zelda (USA, Europe)-0.png>) | [✅](<accuracy_screenshots/VBA-M/Classic NES Series - The Legend of Zelda (USA, Europe).png>) | |
| Digimon Racing | [✅](accuracy_screenshots/SkyEmu/digimon.png) | [✅](accuracy_screenshots/NBA/DigimonRacing.png) | [✅](<accuracy_screenshots/mGBA/Digimon Racing (Europe) (En,Fr,De,Es,It)-0.png>) | [❌](accuracy_screenshots/VBA-M/digimon.png) | VBA-M locks up before the title screen |
| Hello Kitty Collection: Miracle Fashion Maker | [✅](accuracy_screenshots/SkyEmu/hellokitty.png) | [✅](accuracy_screenshots/NBA/HelloKitty.png) | [✅](<accuracy_screenshots/mGBA/Hello Kitty Collection - Miracle Fashion Maker (Japan)-0.png>) | [❌](accuracy_screenshots/VBA-M/hello-kitty.png) | VBA-M does not boot |
| Iridion 3D | [✅](accuracy_screenshots/SkyEmu/Iridian.png) | [✅](accuracy_screenshots/NBA/iridion3D.png) | [❌](accuracy_screenshots/mGBA/Iridion.png) | [❌](accuracy_screenshots/VBA-M/Iridion.png) | mGBA and VBA-M show rendering corruption |
| James Pond: Codename Robocod | [✅](accuracy_screenshots/SkyEmu/JamesPond.png) | [✅](<accuracy_screenshots/NBA/James Pond.png>) | [✅](<accuracy_screenshots/mGBA/James Pond - Codename Robocod (Europe) (En,Fr,De,Es,It,Nl,Pt) 2-0.png>) | [✅](<accuracy_screenshots/VBA-M/James Pond - Codename Robocod (Europe) (En,Fr,De,Es,It,Nl,Pt) 2.png>) | |
| Lufia: The Ruins of Lore | [✅](accuracy_screenshots/SkyEmu/Lufia.png) | [✅](accuracy_screenshots/NBA/Lufia.png) | [✅](<accuracy_screenshots/mGBA/Lufia - The Ruins of Lore (USA)-0.png>) | [❌](accuracy_screenshots/VBA-M/Lufia.png) | VBA-M shows rendering corruption |
| Pinball Tycoon | [✅](accuracy_screenshots/SkyEmu/Pinball-Tycoon.png) | [✅](accuracy_screenshots/NBA/PinballTycoon.png) | [✅](<accuracy_screenshots/mGBA/Pinball Tycoon (U)-0.png>) | [❌](accuracy_screenshots/VBA-M/PinballTycoon.png) | VBA-M shows rendering corruption |
| Sennen Kazoku | [✅](accuracy_screenshots/SkyEmu/Sennen.png) | [✅](accuracy_screenshots/NBA/Sennen.png) | [✅](<accuracy_screenshots/mGBA/Sennen Kazoku (Japan)-0.png>) | [❌](accuracy_screenshots/VBA-M/Sennen.png) | VBA-M does not boot |
| Star Wars Episode II: Attack of the Clones | [✅](accuracy_screenshots/SkyEmu/StarWars.png) | [✅](accuracy_screenshots/NBA/StarWars.png) | [❌](accuracy_screenshots/mGBA/StarWars.png) | [❌](accuracy_screenshots/VBA-M/StarWars.png) | mGBA and VBA-M show rendering corruption |
| **Passed** | **10 / 10** | **10 / 10** | **7 / 10** | **2 / 10** | |

## Test ROMs

| Test | SkyEmu 1.0 | NanoBoyAdvance 1.5 | mGBA 0.9.3 | VBA-M 2.1.4 |
|---|:---:|:---:|:---:|:---:|
| GBA Suite: Memory | ✅ | ✅ | ✅ | ❌ 1338 / 1552 |
| GBA Suite: IO | ✅ | ✅ | ❌ 114 / 123 | ❌ 100 / 123 |
| GBA Suite: Timing | ✅ | ✅ | ❌ 1708 / 2020 | ❌ 751 / 2020 |
| GBA Suite: DMA | ✅ | ✅ | ❌ 1232 / 1256 | ❌ 1032 / 1256 |
| ArmWrestler | ✅ | ✅ | ✅ | ✅ |
| FuzzARM | ✅ | ✅ | ✅ | ✅ |

All screenshots, including more pass frames, are in [`accuracy_screenshots/`](accuracy_screenshots/).
