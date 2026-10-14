# Konami Python 2 Config

## Config

A descriptor is an INI file with a single `[Game]` section.

```ini
[Game]
Name=DDR SuperNova 2
GameId=GDJJA
Region=NTSC-J

HddImagePath=gdj_jaa_2007100800.chd
HddIdPath=ps2_hdd_id.bin
NvRamPath=ps2_nvram.nvm
; IlinkIdPath=ilink_id.bin

DongleBlackPath=ds2430_black_gqgdjjaa.bin
DongleWhitePath=ds2430_white_gqfdhjaa.bin

InputType=2
DIPSW1=false
DIPSW2=false
DIPSW3=false
DIPSW4=false
Force31kHz=false

Player1Card=card1.txt
; Player2Card=card2.txt
PatchFile=ddrsn2j.pnach

ServerUrl=http://127.0.0.1:9550
PcbId=01123456789ABCDEF012
```

## Field reference

| Field                     | Required                     | Description                                              |
|---------------------------|------------------------------|----------------------------------------------------------|
| `Name`                    | Yes                          | Friendly title shown in the game list.                   |
| `GameId`                  | No                           | Game identifier used as the serial.                      |
| `UniqueId`                | No                           | Per-game settings id.                                    |
| `Region`                  | No                           | Game-list region string. Defaults to `NTSC-J`.           |
| `HddImagePath`            | Yes                          | Raw or CHD-compressed HDD image.                         |
| `HddIdPath`               | Yes                          | HDD identity paired image.                               |
| `IlinkIdPath`             | Yes                          | Explicit i.Link ID data.                                 |
| `NvRamPath`               | One identity source required | Console NVRAM. Prefered over i.Link ID file.             |
| `DongleBlackPath`         | Authentication-dependent     | Black Dallas dongle data.                                |
| `DongleWhitePath`         | Authentication-dependent     | White Dallas dongle data.                                |
| `InputType`               | Yes for useful controls      | Input profile. See the mapping below. Defaults to `0`.   |
| `DIPSW1` through `DIPSW4` | No                           | Boolean DIP-switch states. Each defaults to `false`.     |
| `Force31kHz`              | No                           | Reports 31 kHz output through P2IO. Defaults to `false`. |
| `Player1Card`             | No                           | Text file containing a 16-character player card ID.      |
| `Player2Card`             | No                           | Text file containing a 16-character player card ID.      |
| `PatchFile`               | No                           | Extended PNACH patch file loaded.                        |
| `ServerUrl`               | No                           | e-amusement server `http://host:port`. Blank = offline.  |
| `PcbId`                   | No                           | 20-hex-digit e-amusement PCB ID. Blank = white dongle.   |

### Dongle file layouts

The Python 2 dongle loader accepts the layouts commonly found in preservation sets:

- Legacy layout: 8-byte serial ID followed by the 32-byte encrypted payload.
- MAME-style layout: 32-byte encrypted payload followed by the 8-byte serial ID.

Keep the black and white devices associated with the correct title and hardware set.

## Network

With `ServerUrl` set, Python 2 networking is switched on for that game: the Sockets backend with the emulator's own DHCP and DNS. Every DNS answer the cabinet gets is the server's address and every TCP connection it opens goes to the server's host and port, the same way Python 1's `ServerUrl` works. If the host cannot be resolved the cabinet stays offline. With `ServerUrl` blank the game keeps whatever network settings it has. Without an Ethernet backend some games boot offline, but DDR SuperNOVA (Japan) stops in its bootloader, so give it a `ServerUrl`.

`PcbId` is the cabinet's e-amusement PCB ID, the same 20 hex digits used for every other game on the server (the example above is made up; use the one registered for your cabinet). It must start with `01`. The game sends it as `srcid` in every e-amusement request.

On a real cabinet this ID comes from the white dongle: the bootloader builds `01 40` followed by the dongle serial bytes 0, 6 to 1 and 7, and reports it as `sysid`. The emulator therefore signs a white dongle with the last 16 digits of `PcbId`, and patches the bootloader's constant `40` to the second byte of `PcbId` when the game first reads its dongles (the patch is found by an instruction pattern, not a fixed address, and is logged as `P2IO: bootloader PCB ID prefix at ... set to ..`). The bootloader uses the same constant for the black dongle's `softid` (sent as `hardid`), so that ID gets the same second byte. The black dongle itself, the game's license, still comes from `DongleBlackPath`.

Both can also be set per game under **Game Properties > Python 2 > Network Settings**.

Servers often hand out service URLs on `127.0.0.1` (CARDINAL-GATE does in debug mode). The PS2 network stack (SCE INET) keeps any `127.x` traffic to itself, so with `ServerUrl` set the emulator disables that one loopback check in the loaded INET module the first time the game sends a packet (logged as `DEV9: INET loopback check at ... disabled`). Such connections then go out like any other and are redirected to the server.

## Display

Python 2 games draw the full 480 lines, which PCSX2's normal scan window (448 lines) crops. New Python 2 entries therefore default to **Show Overscan** on and **Horizontal Overscan** off (Graphics > Display, per game): the full height without the blank borders at the sides. Both are only defaults; a value set in Game Properties is kept. Horizontal Overscan is used by the paraLLEl-GS renderer.

## Input types

`InputType` selects the P2IO profile and controller bindings:

| Value | Profile |
|---|---|
| `0` | DrumMania |
| `1` | GuitarFreaks |
| `2` | Dance Dance Revolution |
| `3` | Toy's March |
| `4` | Thrill Drive 3 |
| `5` | Dance 86.4 Funky Radio Station |

P2IO is forced onto USB port 1 when a Python 2 game boots. Configure its bindings from that port's device settings.

![Python 2 I/O configuration](p2io-config.png)
