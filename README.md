pgm2xm
------

`pgm2xm` is a tool to extact music from games released for the [IGS PGM arcade hardware](https://wiki.arcadeotaku.com/w/IGS_PolyGame_Master). The sound driver used by most games for the system is in a tracker-based format, which makes it possible to repackage them into tracker modules that can be played on a computer.

Usage
=====

This tool requires a few things:

* A MAME RAM dump produced from a game while a song is playing. For more details, see the "Producing RAM dumps" section.
* The game's sample ROM. For most games, this is the file with the `.u5` extension in the MAME ROMs.
* The BIOS, which contains additional sound samples. This is usually the file with the `.u42` extension in the MAME ROMs. (Not required for CAVE games.)

Call the tool like this:

```
pgm2xm -b z80prg.bin m1300.u5 02 02.xm
```

Where `z80prg.bin` is the filename of your RAM dump, `m1300.u5` is the name of the game's sample ROM, `02` is the ID of the song to dump, and `02.xm` is the output filename.

The `-b` flag specifies whether to load samples from the PGM's BIOS ROM. This isn't required for CAVE games, but is required for games developed by IGS.

Note that some early games for the hardware use a different sound driver and aren't compatible with this tool.

Producing RAM dumps
===================

Getting RAM dumps requires loading a game in MAME with the debugger enabled. For more information about launching it, see the [MAME debugger documentation](https://docs.mamedev.org/debugger/index.html).

When the game is running a song you'd like to dump, open the debugger window and run the following commands:

```
focus soundcpu
save z80prg.bin,0,10000
```

This will produce a file called `z80prg.bin` that you can use with this tool.
