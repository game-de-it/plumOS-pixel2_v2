# Pixel2 ROM set route validation

ROM root: `/Volumes/public-1/02/motoki/emu/ROM/rom2`
App root: `output/app-layer/pixel2/plumos`

## Summary

- enabled systems: 87
- launch profiles: 181
- profile routes OK: 181
- profile routes failed: 0
- systems with representative ROM: 74
- route OK: 74
- standalone pending binary: 0
- systems without matching ROM: 13
- unmapped ROM directories: 01, 3ds, ATARI, _etc, ports, saturn

## Systems with ROMs

| system | sample ROM | default profile | route status | route detail |
| --- | --- | --- | --- | --- |
| nes | `nes/Akumajou Densetsu.nes` | `retroarch:quicknes` | ok | `cores/quicknes_libretro.so` |
| fds | `_etc/fds/勇士の紋章　ディープダンジョンII.fds.zip` | `retroarch:fceumm` | ok | `cores/fceumm_libretro.so` |
| sfc | `snes/Adventures of the Rocketeer.sfc` | `retroarch:snes9x2005` | ok | `cores/snes9x2005_libretro.so` |
| gb | `gb/Aretha (Japan).gb` | `retroarch:gambatte` | ok | `cores/gambatte_libretro.so` |
| gbc | `gbc/Akumanor Gaiden DX v1.0.gbc` | `retroarch:gambatte` | ok | `cores/gambatte_libretro.so` |
| gba | `gba/Castlevania - Aria of Sorrow (European Version - Restored Audio).gba` | `retroarch:gpsp` | ok | `cores/gpsp_libretro.so` |
| megadrive | `megadrive/Bare Knuckle - Ikari no Tekken ~ Streets of Rage (World).md` | `retroarch:genesis_plus_gx` | ok | `cores/genesis_plus_gx_libretro.so` |
| mastersystem | `mastersystem/Frontier Force (V1.1).sms` | `retroarch:genesis_plus_gx` | ok | `cores/genesis_plus_gx_libretro.so` |
| gamegear | `gamegear/Columns [V1.0].gg` | `retroarch:genesis_plus_gx` | ok | `cores/genesis_plus_gx_libretro.so` |
| sega32x | `_etc/sega32x/BC Racers (USA).32x` | `retroarch:picodrive` | ok | `cores/picodrive_libretro.so` |
| segacd | `_etc/segacd/Fatal Fury Special (Europe) (Demo)/Fatal Fury Special (Europe) (Demo) (Track 1).bin` | `retroarch:genesis_plus_gx` | ok | `cores/genesis_plus_gx_libretro.so` |
| pcengine | `pcengine/AirZonk_U.pce` | `retroarch:mednafen_pce_fast` | ok | `cores/mednafen_pce_fast_libretro.so` |
| supergrafx | `_etc/supergrafx/Axelay_SuperGrafx_Demo__(PC_Engine)/Axelay_Demo (SGX).pce` | `retroarch:mednafen_supergrafx` | ok | `cores/mednafen_supergrafx_libretro.so` |
| pcenginecd | `pcenginecd/AVENGER.CUE` | `retroarch:mednafen_pce_fast` | ok | `cores/mednafen_pce_fast_libretro.so` |
| psx | `psx/ART_TRUCK_BATTLE_BAKUSOU_DEKOTORA_DENSETSU.img` | `retroarch:pcsx_rearmed` | ok | `cores/pcsx_rearmed_libretro.so` |
| psp | `psp/Star Soldier (Japan)/Star Soldier (Japan).iso` | `standalone:ppsspp` | ok | `ppsspp` |
| n64 | `n64/AeroGauge [V1.1].z64` | `retroarch:parallel_n64` | ok | `cores/parallel_n64_libretro.so` |
| nds | `nds/99のなみだ バンダイナムコゲームス.nds` | `standalone:drastic` | ok | `drastic` |
| dreamcast | `dreamcast/Crazy Taxi (Japan).chd` | `retroarch:flycast_xtreme` | ok | `cores/flycast_xtreme_libretro.so` |
| neogeo | `_etc/neogeo/ART OF FIGHTING/aof.zip` | `retroarch:fbneo` | ok | `cores/fbneo_libretro.so` |
| neogeocd | `_etc/neogeocd/Fatal Fury WAV/Fatal Fury WAV.cue` | `retroarch:neocd` | ok | `cores/neocd_libretro.so` |
| ngpc | `ngpc/Bakumatsu Rouman Tokubetsu Hen - Gekka no Kenshi - Tsuki ni Saku Hana, Chiri Yuku Hana (Japan).ngc` | `retroarch:mednafen_ngp` | ok | `cores/mednafen_ngp_libretro.so` |
| wonderswan | `wonderswan/Puzzle Bobble.ws` | `retroarch:mednafen_wswan` | ok | `cores/mednafen_wswan_libretro.so` |
| lynx | `ATARI/Lynx/Basketbrawl (1992).lnx` | `retroarch:mednafen_lynx` | ok | `cores/mednafen_lynx_libretro.so` |
| virtualboy | `_etc/viretualboy/Formula V Public Demo (2021-10-14) (Emulators).vb` | `retroarch:mednafen_vb` | ok | `cores/mednafen_vb_libretro.so` |
| arcade | `mame/1942a.zip` | `retroarch:mame2003_plus` | ok | `cores/mame2003_plus_libretro.so` |
| fbneo | `fbneo/airduelm72.zip` | `retroarch:fbneo` | ok | `cores/fbneo_libretro.so` |
| cps1 | `mame/1942a.zip` | `retroarch:fbneo` | ok | `cores/fbneo_libretro.so` |
| cps2 | `mame/ssf2u.zip` | `retroarch:fbneo` | ok | `cores/fbneo_libretro.so` |
| cps3 | `mame/sfiii3nr1.zip` | `retroarch:fbneo` | ok | `cores/fbneo_libretro.so` |
| dos | `pc/DOSBOX_ALIENBREED.ZIP` | `retroarch:dosbox_pure` | ok | `cores/dosbox_pure_libretro.so` |
| easyrpg | `_etc/EASYRPG/TurnedIntoAGirl/RPG_RT.ldb` | `retroarch:easyrpg` | ok | `cores/easyrpg_libretro.so` |
| pico8 | `pico-8/51752.p8` | `retroarch:fake08` | ok | `cores/fake08_libretro.so` |
| scummvm | `scummvm/BASS-Floppy-1.3` | `retroarch:scummvm` | ok | `cores/scummvm_libretro.so` |
| openbor | `openbor/Crisis Evil 1.pak` | `standalone:openbor` | ok | `openbor` |
| msx | `msx2/XGR1Trial.rom` | `retroarch:bluemsx` | ok | `cores/bluemsx_libretro.so` |
| pc88 | `pc88/XeGrader100001.d88` | `retroarch:quasi88` | ok | `cores/quasi88_libretro.so` |
| pc98 | `_etc/pc-9800/Can Can Bunny 5 and half Limited.hdi` | `retroarch:np2kai` | ok | `cores/np2kai_libretro.so` |
| atari2600 | `ATARI/2600/3DTicTacToe.bin` | `retroarch:stella2014` | ok | `cores/stella2014_libretro.so` |
| atari7800 | `ATARI/7800/Asteroids (1987) (Atari).a78` | `retroarch:prosystem` | ok | `cores/prosystem_libretro.so` |
| supervision | `_etc/supervision/assembloids_supervision_2022.bin` | `retroarch:potator` | ok | `cores/potator_libretro.so` |
| odyssey2 | `_etc/odyssey2/Interpol (Europe) (Proto).bin` | `retroarch:o2em` | ok | `cores/o2em_libretro.so` |
| gameandwatch | `_etc/gameandwatch/Snoopy (Nintendo, Table Top).zip` | `retroarch:gw` | ok | `cores/gw_libretro.so` |
| pokemini | `_etc/pokemini/Pichu Bros. Mini (Japan).zip` | `retroarch:pokemini` | ok | `cores/pokemini_libretro.so` |
| doom | `_etc/doom/freedoom-0.13.0/freedoom1.wad` | `retroarch:prboom` | ok | `cores/prboom_libretro.so` |
| pyxel | `pyxel/LastEmulator.pyxapp` | `pyxel:pixel2` | ok | `pyxel` |
| 3do | `_etc/3do/biofury.iso` | `retroarch:opera` | ok | `cores/opera_libretro.so` |
| amiga | `_etc/amiga/3rd Day - The Ripper (19xx)(Action Force).adf` | `retroarch:puae` | ok | `cores/puae_libretro.so` |
| atari5200 | `ATARI/5200/BountyBobStrikesBack.a52` | `retroarch:atari800` | ok | `cores/atari800_libretro.so` |
| atari800 | `ATARI/800/ATARIBAS.ROM` | `retroarch:atari800` | ok | `cores/atari800_libretro.so` |
| atarist | `_etc/atarist/Lattice C (19xx)(Hisoft).st` | `retroarch:hatari` | ok | `cores/hatari_libretro.so` |
| c64 | `_etc/c64/Pacmania.zip` | `retroarch:vice_x64` | ok | `cores/vice_x64_libretro.so` |
| cannonball | `_etc/cannonball/outrun-roms-master/epr-10187.88` | `retroarch:cannonball` | ok | `cores/cannonball_libretro.so` |
| cavestory | `_etc/cavestory/doukutsu/Doukutsu.exe` | `retroarch:nxengine` | ok | `cores/nxengine_libretro.so` |
| chailove | `_etc/chailove/FloppyBird.chailove` | `retroarch:chailove` | ok | `cores/chailove_libretro.so` |
| channelf | `_etc/channelf/tents_CF.bin` | `retroarch:freechaf` | ok | `cores/freechaf_libretro.so` |
| colecovision | `_etc/coleco/Dam Busters, The (USA).col` | `retroarch:bluemsx` | ok | `cores/bluemsx_libretro.so` |
| cpc | `_etc/cpc/007 - Live and Let Die (1988)(Domark).dsk` | `retroarch:crocods` | ok | `cores/crocods_libretro.so` |
| dinothawr | `_etc/dinothawr/dinothawr/dinothawr.game` | `retroarch:dinothawr` | ok | `cores/dinothawr_libretro.so` |
| intellivision | `_etc/intellivision/Lock 'N' Chase (USA, Europe) (v1.1)/Lock 'N' Chase (USA, Europe) (v1.1).bin` | `retroarch:freeintv` | ok | `cores/freeintv_libretro.so` |
| j2me | `_etc/j2me/Cento.jar` | `retroarch:squirreljme` | ok | `cores/squirreljme_libretro.so` |
| jaguar | `ATARI/Jaguar/AtariKarts.j64` | `retroarch:virtualjaguar` | ok | `cores/virtualjaguar_libretro.so` |
| lowresnx | `_etc/lowresnx/Rubik's cube.nx` | `retroarch:lowresnx` | ok | `cores/lowresnx_libretro.so` |
| lutro | `_etc/lutro/Pong-buildbot-alpha255.lutro` | `retroarch:lutro` | ok | `cores/lutro_libretro.so` |
| microw8 | `_etc/microw8/control.uw8` | `retroarch:uw8` | ok | `cores/uw8_libretro.so` |
| music | `_etc/music/blueshadow.nsfe` | `retroarch:gme` | ok | `cores/gme_libretro.so` |
| pcfx | `_etc/pcfx/SimpleBattle_FX/simplebattle.cue` | `retroarch:mednafen_pcfx` | ok | `cores/mednafen_pcfx_libretro.so` |
| quake | `_etc/quake/Quake Shareware (1_06) PAK/PAK0.PAK` | `retroarch:tyrquake` | ok | `cores/tyrquake_libretro.so` |
| thomson | `_etc/moto/3d Sub (1985)(Loriciels).k7` | `retroarch:theodore` | ok | `cores/theodore_libretro.so` |
| ti83 | `_etc/ti83/tetris.8xp` | `retroarch:numero` | ok | `cores/numero_libretro.so` |
| uzebox | `_etc/uzebox/Arkanoid2.uze` | `retroarch:uzem` | ok | `cores/uzem_libretro.so` |
| vic20 | `_etc/vic20/tetwels.prg` | `retroarch:vice_xvic` | ok | `cores/vice_xvic_libretro.so` |
| vmu | `_etc/vmu/ANIMTEST.VMI` | `retroarch:vemulator` | ok | `cores/vemulator_libretro.so` |
| zxspectrum | `_etc/zxspectrum/10th Frame (1987)(U.S. Gold).tap` | `retroarch:fuse` | ok | `cores/fuse_libretro.so` |

## Systems without matching ROM

ngp, wonderswancolor, x68000, tic80, vectrex, sg1000, sharpx1, wolf3d, zx81, arduboy, megaduck, puzzlescript, superbroswar

## Failed launch-profile routes

none
