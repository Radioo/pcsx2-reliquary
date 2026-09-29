# Konami Python 1 Config

## Config

A descriptor is an INI file with a single `[Game]` section.

```ini
[Game]
Name=Pop'n Music 14
GameId=GNF14JAB
Region=NTSC-J

HddImagePath=popn14/popn14.chd
BbsRamPath=popn14/m48t58y.u48
IoBootRomPath=popn14/b22a01.u42
IoConfigRomPath=popn14/d72872gc.crom
InternalDonglePath=popn14/ds2430.u3
ExternalDonglePath=popn14/ds2430_black_gnf14jab.u3
MemoryCardDonglePath=popn14/kn00002.ps2
MemoryCardIdPath=popn14/kn00002.id
CardNumber=0000000000000001
IoMode=POPN
```

Use either `HddImagePath` or `CfImagePath` for normal game media. A descriptor may contain both, such as an update kit.

## Field reference

| Field                  | Required | Description                                                                                                                                                                                        |
|------------------------|----------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `Name`                 | Yes      | Friendly title shown in the game list. Defaults to the descriptor path.                                                                                                                            |
| `GameId`               | No       | Game identifier used as the serial. Reliquary attempts to extract it from the P1IO boot ROM.                                                                                                       |
| `UniqueId`             | No       | Per-game settings id.                                                                                  |
| `Region`               | No       | Game-list region string. Defaults to `NTSC-J`.                                                                                                                                                     |
| `HddImagePath`         | Yes      | Raw or CHD-compressed HDD image.                                                                                                                                                                   |
| `CfImagePath`          | Yes      | Raw or CHD-compressed CompactFlash image. PythonFS inside the MBR/FAT16 container is handled by the emulated platform path.                                                                        |
| `BbsRamPath`           | Yes      | Writable path for the P1IO's 8 KiB battery-backed SRAM. A zero-filled file is created if the path does not exist. Some titles can initialize blank state; others require matching dumped contents. |
| `IoBootRomPath`        | Yes      | P1IO boot ROM.                                                                                                                                                                                     |
| `IoConfigRomPath`      | Yes      | FireWire configuration ROM, board OUI.                                                                                                                                                             |
| `InternalDonglePath`   | Maybe    | Internal DS2430 data.                                                                                                                                                                              |
| `ExternalDonglePath`   | Maybe    | External round black dongle.                                                                                                                                                                       |
| `MemoryCardDonglePath` | Yes      | Raw PS2 memory-card dongle image including ECC/spare data. It is assigned to memory-card slot 1.                                                                                                   |
| `MemoryCardIdPath`     | Yes      | Card ID/key data used for card-bound KELF auth.                                                                                                                                                    |
| `IoMode`               | No       | P1IO protocol/input profile. Defaults to `JVS`.                                                                                                                                                    |
| `CardNumber`           | No       | 16 hex digits for the pop'n card reader. When the game is first added to the game list, a registered pop'n 9 card with this number is created in the card manager (Tools > Pop'n Card Manager) and put in the game's reader. After that, cards are managed only in the card manager. Only used with `IoMode=POPN`. |

## I/O modes

`IoMode` is case-insensitive when the descriptor is scanned and is normalized into one of these values:

| Value          | Profile                    |
|----------------|----------------------------|
| `JVS`          | General JVS-oriented P1IO  |
| `EXTIO`        | Dancing Stage Fusion       |
| `POPN`         | Pop'n Music                |
| `PPOOL`        | Perfect Pool               |
| `B22`          | Dogstation B22 I/O profile |
| `DOGSTATIONDX` | DogStation DX I/O profile  |

Select the mode matching the board firmware and game. The setting affects P1IO protocol responses as well as which automatic controller bindings are exposed.

## P1IO controls

Configure Python 1 inputs under the FireWire section of the controller settings.

![Python 1 I/O configuration](p1io-config.png)

## Pop'n cards

Cards live in `memcards/popn_cards/`, one 128-byte `.bin` file per card, named after the card. Each game's settings pick one card for its reader (`Python1/Game/CardFile`). The game reads the card from that file when it is inserted and writes its changes back to the same file.

- **Tools > Pop'n Card Manager** lists every card with the game it belongs to, its state, number, card data, checksums, the games that use it and its raw bytes. It creates, copies, renames, imports, exports, repairs and deletes cards.
- **Game Properties > Python 1 > Card** picks the card in that game's reader.
- **System > Card Reader** inserts the game's card, or any other card for this session.

`PCSX2_FW_POPN_CARD_FILE` overrides `CardFile` with a card name.

The old `CardNumber` and `CardDesign` game settings, and the old `python1_popn_card_<serial>.bin` files, are turned into cards the first time the app starts.
