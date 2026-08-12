# Pixel2 ROM set route validation

ROM root: `/Volumes/public-1/02/motoki/emu/ROM/rom2`
App root: `output/app-layer/pixel2/plumos`

## Summary

- enabled systems: 87
- systems with representative ROM: 29
- route OK: 28
- standalone pending binary: 1
- systems without matching ROM: 58
- unmapped ROM directories: 01, 3ds, ATARI, _etc, ports, pyxel

## Systems with ROMs

| system | sample ROM | default profile | route status | route detail |
| --- | --- | --- | --- | --- |
| nes | `nes/Akumajou Densetsu.nes` | `retroarch:quicknes` | ok | `cores/quicknes_libretro.so` |
| sfc | `snes/Adventures of the Rocketeer.sfc` | `retroarch:snes9x2005` | ok | `cores/snes9x2005_libretro.so` |
| gb | `gb/Aretha (Japan).gb` | `retroarch:gambatte` | ok | `cores/gambatte_libretro.so` |
| gbc | `gbc/Akumanor Gaiden DX v1.0.gbc` | `retroarch:gambatte` | ok | `cores/gambatte_libretro.so` |
| gba | `gba/Castlevania - Aria of Sorrow (European Version - Restored Audio).gba` | `retroarch:gpsp` | ok | `cores/gpsp_libretro.so` |
| megadrive | `megadrive/Bare Knuckle - Ikari no Tekken ~ Streets of Rage (World).md` | `retroarch:genesis_plus_gx` | ok | `cores/genesis_plus_gx_libretro.so` |
| mastersystem | `mastersystem/Frontier Force (V1.1).sms` | `retroarch:genesis_plus_gx` | ok | `cores/genesis_plus_gx_libretro.so` |
| gamegear | `gamegear/Columns [V1.0].gg` | `retroarch:genesis_plus_gx` | ok | `cores/genesis_plus_gx_libretro.so` |
| pcengine | `pcengine/AirZonk_U.pce` | `retroarch:mednafen_pce_fast` | ok | `cores/mednafen_pce_fast_libretro.so` |
| pcenginecd | `pcenginecd/AVENGER.CUE` | `retroarch:mednafen_pce_fast` | ok | `cores/mednafen_pce_fast_libretro.so` |
| psx | `psx/ART_TRUCK_BATTLE_BAKUSOU_DEKOTORA_DENSETSU.img` | `retroarch:pcsx_rearmed` | ok | `cores/pcsx_rearmed_libretro.so` |
| psp | `psp/Star Soldier (Japan)/Star Soldier (Japan).iso` | `standalone:ppsspp` | pending-binary | `ppsspp` |
| n64 | `n64/AeroGauge [V1.1].z64` | `retroarch:parallel_n64` | ok | `cores/parallel_n64_libretro.so` |
| nds | `nds/99のなみだ バンダイナムコゲームス.nds` | `standalone:drastic` | ok | `drastic` |
| dreamcast | `dreamcast/Crazy Taxi (Japan).chd` | `retroarch:flycast_xtreme` | ok | `cores/flycast_xtreme_libretro.so` |
| saturn | `saturn/BLACK_MATRIX.BIN` | `retroarch:yabasanshiro` | ok | `cores/yabasanshiro_libretro.so` |
| ngpc | `ngpc/Bakumatsu Rouman Tokubetsu Hen - Gekka no Kenshi - Tsuki ni Saku Hana, Chiri Yuku Hana (Japan).ngc` | `retroarch:mednafen_ngp` | ok | `cores/mednafen_ngp_libretro.so` |
| wonderswan | `wonderswan/Puzzle Bobble.ws` | `retroarch:mednafen_wswan` | ok | `cores/mednafen_wswan_libretro.so` |
| arcade | `mame/1942a.zip` | `retroarch:mame2003_plus` | ok | `cores/mame2003_plus_libretro.so` |
| fbneo | `fbneo/airduelm72.zip` | `retroarch:fbneo` | ok | `cores/fbneo_libretro.so` |
| dos | `pc/DOSBOX_ALIENBREED.ZIP` | `retroarch:dosbox_pure` | ok | `cores/dosbox_pure_libretro.so` |
| pico8 | `pico-8/51752.p8` | `retroarch:fake08` | ok | `cores/fake08_libretro.so` |
| openbor | `openbor/Crisis Evil 1.pak` | `standalone:openbor` | ok | `openbor` |
| msx | `msx2/XGR1Trial.rom` | `retroarch:bluemsx` | ok | `cores/bluemsx_libretro.so` |
| pc88 | `pc88/XeGrader100001.d88` | `retroarch:quasi88` | ok | `cores/quasi88_libretro.so` |
| atari2600 | `ATARI/2600/3DTicTacToe.bin` | `retroarch:stella2014` | ok | `cores/stella2014_libretro.so` |
| atari7800 | `ATARI/7800/Asteroids (1987) (Atari).a78` | `retroarch:prosystem` | ok | `cores/prosystem_libretro.so` |
| atari5200 | `ATARI/5200/BountyBobStrikesBack.a52` | `retroarch:atari800` | ok | `cores/atari800_libretro.so` |
| atari800 | `ATARI/800/ATARIBAS.ROM` | `retroarch:atari800` | ok | `cores/atari800_libretro.so` |

## Systems without matching ROM

fds, sega32x, segacd, supergrafx, neogeo, neogeocd, ngp, wonderswancolor, lynx, virtualboy, cps1, cps2, cps3, easyrpg, scummvm, pc98, x68000, tic80, vectrex, supervision, odyssey2, gameandwatch, pokemini, doom, 3do, amiga, atarist, c64, cannonball, cavestory, chailove, channelf, colecovision, cpc, dinothawr, intellivision, j2me, jaguar, lowresnx, lutro, microw8, music, pcfx, quake, sg1000, sharpx1, thomson, ti83, uzebox, vic20, vmu, wolf3d, zx81, zxspectrum, arduboy, megaduck, puzzlescript, superbroswar
