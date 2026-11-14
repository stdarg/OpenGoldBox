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

## Playing the music

Since 2026-11-14 OpenGoldBox plays a bank's song as the driver plays it on an
AdLib card: `render_music` (`music_render.h`) runs the driver's sequencer and
AdLib routines, worked out from the driver's code in all three builds, and
writes their register values to `FmSynth` (`fm_synth.h`), our own model of the
card's chip. `load_music_bank` reads a piece from the game's archive and
`SoundBank::load_music` renders it into a clip, so music plays through the same
`SoundPlayer` and Godot output as the sound effects.

### What the driver does on the AdLib

- **Which song.** Starting the driver always plays the bank's first song (the
  number the game passes picks only an echo setting, below). Every bank holds
  one song.
- **Which tracks.** On the PC speaker the driver plays track 0 alone; on every
  other card it plays tracks 1 to 3, one voice each. Voice *n* plays on the
  chip's channel *n*: the driver gives each voice the first free channel, and
  the instruments' channel byte, which could hold one to a channel, is 0 in
  every bank.
- **Controls.** 81h releases the voice and loads an AdLib instrument into its
  channel, always the instrument numbered like the voice (0 to 2); the
  control's value is not read. 84h sets the voice's velocity to 7Fh. The
  driver sets the chip's rhythm mode, but no voice reaches its drum channels.
- **Loading an instrument.** For each operator the driver writes registers
  40h, 60h, 80h and 20h from the instrument, and C0h with the first operator.
  It never sets a waveform: at the start it clears every operator's waveform
  register and register 01h, so every operator plays a sine.
- **A note.** The driver first releases the channel, then sets the carrier's
  level from the velocity (the instrument's level counts as full: the level
  is 3Fh less (3Fh less the instrument's level) times the velocity over 128,
  the key-scale bits kept), then the pitch, and strikes. A note's block is its
  octave less one and its frequency number comes from a table of twelve (157h
  for C to 28Ah for B), so note 60 is middle C (260 Hz on the chip).
- **Releasing.** To release a note the driver clears the key bit and sets the
  channel's frequency to 24Ah in block 3, so a slowly fading note slides to
  that pitch. A rest releases; a tied note's successor neither strikes nor
  changes the pitch; a note followed by the same pitch is released one tick
  early.
- **The end.** When every track has reached its end, the driver releases all
  nine channels and stops: the song plays once. (Its code can start a song
  again, but no call of the games asks for it.)
- **Echo.** The driver can double a voice on a spare channel, later and
  slightly detuned, with settings chosen by the number the game passes when it
  starts the music. Every game passes 0, which leaves it off, so OpenGoldBox
  leaves it out.

The driver moves the music in the 72.8 Hz ticks of `decode_music_bank`; the
renderer steps every tick (the driver checks every other one, which shifts a
note by at most one tick). After the end the last notes fade out until they
fall below hearing, at most three seconds.

### What FmSynth covers

`FmSynth` takes the chip's register writes and makes samples at any rate. It
follows the behaviour the chip's documentation and its well-known traits
describe, computed in decibels and cycles rather than in the chip's own
integer steps:

- nine channels of two operators, the modulator shifting the carrier's phase
  (four cycles at full level), or both heard together when C0h's bit 0 is set;
- the modulator's feedback, the mean of its last two outputs, from 1/32 of a
  cycle at feedback 1 to two cycles at 7;
- each operator's pitch from the frequency number, block and multiple (½, 1
  to 10, 12 and 15), restarted at each key-on;
- the envelope: an exponential attack, a decay to the sustain level (3 dB
  steps), held while the key is down for a sustained sound, else falling at
  the release rate, and the release; each rate's time is the documented time
  for rate 1 (2826 ms to attack, 39 281 ms to decay 96 dB) halved for every
  step, the key scaling's offset (from the block and the frequency number's
  top bit picked by register 08h) adding quarter steps;
- the total level (0.75 dB steps), key scaling of the level (none, 1.5, 3 or
  6 dB an octave), tremolo (1 or 4.8 dB at 3.7 Hz) and vibrato (7 or 14 cents
  at 6.1 Hz), the depths from register BDh.

Left out, as no bank or driver uses them: the waveforms other than the sine
(register 01h keeps them off), the rhythm section's drums, the timers and
composite-sine mode. The output is the plain sum of the channels; the chip's
own rounding, its 9-bit sine and exponent tables and its DAC are not imitated.
A single operator at full level reaches a quarter of the 16-bit range.

### Where the games play it

| Game | Piece | When |
|---|---|---|
| *Secret* | `SOUND.DAX` record 2 | Over the title pictures and credits |
| *Gateway* | `MUSIC.DAX` record 1 | Over the title pictures, which wait for its end |
| *Pools of Darkness*, *Treasures* | `MUSIC.DAX` record 1 | Over the title pictures |
| *Gateway*, *Pools of Darkness*, *Treasures* | `MUSIC.DAX` record *n* | A script's PLAY SOUND with the value 80h + *n* |

`title_music_piece` gives the title's piece. A new piece stops the one
playing; a sound effect stops the music in *Pools of Darkness* and
*Treasures*.

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
