# Pool of Radiance EGA encounter and combat images

The initial vertical slice read `SPRIT1.DAX` encounter art at runtime. No decoded artwork is
stored in this repository.

A DAX file begins with a little-endian 16-bit table size. Each nine-byte table
entry contains a one-byte record ID, a four-byte data offset relative to the end
of the table, a two-byte expanded size, and a two-byte stored size.

Records use signed-byte RLE. Non-negative command `n` copies the next `n + 1`
bytes literally. Negative command `-n` repeats the following byte `n` times.

An EGA dependent-image sprite record begins with its frame count. Each frame has
a 21-byte header: delay (4), height (2), byte-width (2), x and y offsets (2 each),
one metadata byte, and eight CGA mapping bytes. Pixel data follows at four bytes
per byte-width per row. Each byte holds two 4-bit EGA palette indices, high
nibble first. For `SPRIT` resources palette index zero is transparent.
Zero dimensions, truncated frames, and byte-widths whose pixel width would
overflow the image's 16-bit width are rejected before allocating the output.

## Close-up animations

`PIC1.DAX` to `PIC8.DAX` hold the 88x88 close-ups that fill the 3D view. Every
record is an animation: a frame-count byte, then per frame a 32-bit delay and a
17-byte-header picture (see below). The first frame is complete; each later
frame stores only the XOR difference from the **first** frame, not from the
previous one. Delays count ticks of the PC's 18.2 Hz timer, and zero-tick
frames are never held on screen. The Slums orc (PIC2 record 4) has delays
15, 0, 7, 0, so it alternates a ready and a striking pose.

`SETUP MONSTER sprite, distance, picture` selects both the approach sprite
(SPRITn) and this close-up. After the approach, the game loops the close-up
until a key press, then opens combat. Records 1 (treasure chest) and 29
(campfire) appear in every PIC archive.

## Combat images

The demo also reads `CPIC*.DAX`, `COMSPR.DAX`, `CHEAD.DAX`, and `CBODY.DAX`.
These use a 17-byte record header: height (uint16), byte-width (uint16), x and y
offsets (uint16 each), image count (uint8), and eight mapping bytes. Each stored
image occupies `height * byte_width * 4` bytes, with the same packed-nibble
pixel layout. The decoder validates the header dimensions and exact payload
length before reading pixels. The viewer displays native image rectangles;
placement offsets are not used to compose heads/bodies or game scenes.

Combat palette index 0 is transparent, index 8 is black, and index 13 is bright
magenta. Encounter sprites instead use dark gray at index 8 and black at index
13. See [ART.md](ART.md) for category roles and coverage, and the linked Gold
Box Explorer references for the documented format/palette distinction.
