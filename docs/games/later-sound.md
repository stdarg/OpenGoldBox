# The later games' music and digitized sounds

*Secret of the Silver Blades*, *Gateway to the Savage Frontier*, *Pools of
Darkness* and *Treasures of the Savage Frontier* play their music through one
music driver, and the last three play digitized sound effects. This page
records how those files are laid out, as the census (`opengold_census`,
[GAMES.md](../GAMES.md)) reads them. No reconstructed source covers the music
driver; its layout comes from the files and from the driver program's own
code. The wording is our own.

Since 2026-11-13 every sound and music file of the four games decodes.

## The files

| Game | File | What it holds |
|---|---|---|
| *Secret* | `SOUND.DAX` | The driver (record 1) and the title music's bank (2) |
| *Gateway* | `MUSIC.DAX` | The driver (0) and one bank (1) |
| *Pools of Darkness* | `MUSIC.DAX` | The driver (0) and eight banks (1 to 8) |
| *Pools of Darkness* | `SOUND.COM`, `P1.SMS`, `P3.SMS` | The driver and banks 1 and 3 again, for the introduction and the finale |
| *Treasures* | `MUSIC.DAX` | The driver (0) and one bank (1) |
| *Pools of Darkness*, *Treasures* | `ZOUNDS.DAX` | Eleven digitized sound effects (records 1 to 11) |

## The music driver

The driver is a small program built for DOS as a `.COM` file. The game copies
it to the start of a paragraph and calls its far entry with a function number:
0 installs it with a bank, 1 starts the song, 3 stops it. The four games carry
three builds of it (*Pools of Darkness* and *Treasures* the same bytes); each
plays on an AdLib, a Creative Music System, an Innovation SSI-2001, a Covox,
the Tandy's sound chip or the PC speaker. OpenGoldBox does not run it: the
census recognizes it by its entry and its message for a bad bank, and reads the
banks instead.

The driver sets the PC's timer to 72.8 ticks a second (a divisor of 16384) and
moves the music on every other tick.

## A bank

A bank is the parameter block the game hands the driver. Its numbers are
stored high byte first. It has two parts.

**The instrument library**, tagged `GLBR`: the tag, the library's length in
bytes (4 bytes, counting the tag), a word the driver ignores (FFFFh), the
number of instruments, then each instrument's size and the instruments
themselves. They come in groups of eight, one group a sound card:

| Instruments | Card | Size |
|---|---|---|
| 0–7 | AdLib | 12 bytes |
| 8–15 | Creative Music System | 18 bytes |
| 16–23 | Innovation | 8 bytes |
| 24–31 | Covox | 8 bytes |

The Tandy and the PC speaker use none. *Secret*'s library stops at 31
instruments. An AdLib instrument holds a byte the driver does not read, the
channel it may be held to, then the two operators' values for the chip's
registers 20h, 40h, 60h and 80h (operator one first), the value for C0h, and a
last byte the driver does not read.

**The songs**, tagged `SLBR`, right after the library: the tag, the part's
length (counting the tag), the number of songs, a zero byte, then one more
offset than there are songs, each from the end of that list, the last marking
the end of the songs. The driver finds the songs at a fixed place, the length
of the library its own build expects (1BCh, or 1B2h in *Secret*'s); every bank
matches its driver. Some files carry a few stray bytes after the songs.

A song starts with its tempo (2 bytes), a starting velocity (7Fh), its number
of tracks (four in every file), and five offsets from the end of this 14-byte
header: where each track starts, then the end. A track is a list of two-byte
events:

| First byte | Event | Second byte |
|---|---|---|
| 00h–7Fh | A note, in semitones (twelve to an octave) | Its length |
| 80h | A rest | Its length |
| 81h–FEh | A control, taking no time | Its value |
| FFh | The end of the track | FFh |

The driver acts on two controls: 81h hands the track's voice to the card's own
routine, and 84h sets the velocity back to 7Fh (its value is ignored). The files also hold
82h, 83h, 87h and 88h, which the driver skips.

A note's length is counted in units, a whole note being 6900h (26,880):

- bits 0 to 2 halve it that many times;
- bit 3 makes it dotted, half as long again;
- bits 4 and 5, read as *n* = 2, 4 or 6 for the values 1, 2, 3, play it as *n*
  of *n* + 1 equal parts (two thirds for a triplet);
- bit 6 ties it to the next note: that note does not strike again, and the
  sound keeps the tied note's pitch;
- bit 7 is not read.

The driver turns units into ticks by dividing by a divisor and carrying the
remainders from note to note, so a track keeps time over its whole length. The
later drivers divide the song's tempo by 84 for the divisor (tempo 2300h gives
106); *Secret*'s driver ignores the tempo and always uses 118. With that, the
four tracks of a song end within a few ticks of one another: *Secret*'s title
music runs 6150 ticks (84 seconds), *Gateway*'s 3973 and *Treasures*' 3544.

When a note is followed by the same pitch, the driver releases it one tick
early so that the next one strikes anew.

`decode_music_bank` reads a bank into instruments and songs, every event timed
in ticks; `is_music_driver` recognizes the driver.

## The digitized sound effects

`ZOUNDS.DAX` of *Pools of Darkness* and *Treasures* holds unsigned 8-bit
samples, as *Gateway*'s does, but the games use them differently: they load
records 1 to 11 at the start, sound *n* (2 to 12) plays record *n* − 1, and
every effect plays at 9000 samples a second, the whole record copied to a
9000-byte buffer. The two games share every effect but record 9 (*Pools of
Darkness*' is an effect of 1772 bytes, *Treasures*' a click of 132).

Every record but 4 (and, in *Treasures*, 9) is an Amiga sound file (`FORM`,
`8SVX`) with the top bit of every byte flipped, the header included: the
conversion to unsigned samples was applied to the whole file. The headers name
rates from 1772 to 10,980 samples a second, which the games ignore, and the
resident drivers do not read the header, so the original plays its 104 bytes
as a short click before the sound. OpenGoldBox fixes this
([DEVIATIONS.md](../DEVIATIONS.md), Sound): `gateway::decode_later_sound_sample`
gives a sound file's samples alone (its `BODY`) at the rate its header (`VHDR`)
names, and any other record whole at 9000 a second.
