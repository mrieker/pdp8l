//    Copyright (C) Mike Rieker, Beverly, MA USA
//    www.outerworldapps.com
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; version 2 of the License.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    EXPECT it to FAIL when someone's HeALTh or PROpeRTy is at RISk.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//    http://www.gnu.org/licenses/gpl-2.0.html

// Reads MIDI file from stdin then writes MU file to stdout

//    cc -Wall -g -o readmidi readmidi.c

//  ./readmidi [-midi32 <value>] [-qpm <value>] [-verbose] < midifile > mutxtfile
//    -midi32 = midi ticks per 32nd note (default 120)
//    -qpm = quarter notes per minute (default 60)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FOURCHARS(s) ((s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3])

// https://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html
// https://ccrma.stanford.edu/~craig/14q/midifile/MidiFileFormat.html

// "4C" = middle C ~= 266Hz
// "4A" = 440Hz
static char const *const midkeystr[256] = {
    "-1C ", "-1C#", "-1D ", "-1D#", "-1E ", "-1F ", "-1F#", "-1G ", "-1G#", "-1A ", "-1A#", "-1B ",
    " 0C ", " 0C#", " 0D ", " 0D#", " 0E ", " 0F ", " 0F#", " 0G ", " 0G#", " 0A ", " 0A#", " 0B ",
    " 1C ", " 1C#", " 1D ", " 1D#", " 1E ", " 1F ", " 1F#", " 1G ", " 1G#", " 1A ", " 1A#", " 1B ",
    " 2C ", " 2C#", " 2D ", " 2D#", " 2E ", " 2F ", " 2F#", " 2G ", " 2G#", " 2A ", " 2A#", " 2B ",
    " 3C ", " 3C#", " 3D ", " 3D#", " 3E ", " 3F ", " 3F#", " 3G ", " 3G#", " 3A ", " 3A#", " 3B ",
    " 4C ", " 4C#", " 4D ", " 4D#", " 4E ", " 4F ", " 4F#", " 4G ", " 4G#", " 4A ", " 4A#", " 4B ",
    " 5C ", " 5C#", " 5D ", " 5D#", " 5E ", " 5F ", " 5F#", " 5G ", " 5G#", " 5A ", " 5A#", " 5B ",
    " 6C ", " 6C#", " 6D ", " 6D#", " 6E ", " 6F ", " 6F#", " 6G ", " 6G#", " 6A ", " 6A#", " 6B ",
    " 7C ", " 7C#", " 7D ", " 7D#", " 7E ", " 7F ", " 7F#", " 7G ", " 7G#", " 7A ", " 7A#", " 7B ",
    " 8C ", " 8C#", " 8D ", " 8D#", " 8E ", " 8F ", " 8F#", " 8G ", " 8G#", " 8A ", " 8A#", " 8B ",
    " 9C ", " 9C#", " 9D ", " 9D#", " 9E ", " 9F ", " 9F#", " 9G ",
    "0x80", "0x81", "0x82", "0x83","0x84", "0x85", "0x86", "0x87","0x88", "0x89", "0x8A", "0x8B","0x8C", "0x8D", "0x8E", "0x8F",
    "0x90", "0x91", "0x92", "0x93","0x94", "0x95", "0x96", "0x97","0x98", "0x99", "0x9A", "0x9B","0x9C", "0x9D", "0x9E", "0x9F",
    "0xA0", "0xA1", "0xA2", "0xA3","0xA4", "0xA5", "0xA6", "0xA7","0xA8", "0xA9", "0xAA", "0xAB","0xAC", "0xAD", "0xAE", "0xAF",
    "0xB0", "0xB1", "0xB2", "0xB3","0xB4", "0xB5", "0xB6", "0xB7","0xB8", "0xB9", "0xBA", "0xBB","0xBC", "0xBD", "0xBE", "0xBF",
    "0xC0", "0xC1", "0xC2", "0xC3","0xC4", "0xC5", "0xC6", "0xC7","0xC8", "0xC9", "0xCA", "0xCB","0xCC", "0xCD", "0xCE", "0xCF",
    "0xD0", "0xD1", "0xD2", "0xD3","0xD4", "0xD5", "0xD6", "0xD7","0xD8", "0xD9", "0xDA", "0xDB","0xDC", "0xDD", "0xDE", "0xDF",
    "0xE0", "0xE1", "0xE2", "0xE3","0xE4", "0xE5", "0xE6", "0xE7","0xE8", "0xE9", "0xEA", "0xEB","0xEC", "0xED", "0xEE", "0xEF",
    "0xF0", "0xF1", "0xF2", "0xF3","0xF4", "0xF5", "0xF6", "0xF7","0xF8", "0xF9", "0xFA", "0xFB","0xFC", "0xFD", "0xFE", "0xFF"
};

// "C" = middle C ~= 266Hz
// "A+" = 440Hz
static char const *const muskeystr[] = {
    "C-----", "C#-----", "D-----", "D#-----", "E-----", "F-----", "F#-----", "G-----", "G#-----", "A----",  "A#----",  "B----",
    "C----",  "C#----",  "D----",  "D#----",  "E----",  "F----",  "F#----",  "G----",  "G#----",  "A---",   "A#---",   "B---",
    "C---",   "C#---",   "D---",   "D#---",   "E---",   "F---",   "F#---",   "G---",   "G#---",   "A--",    "A#--",    "B--",
    "C--",    "C#--",    "D--",    "D#--",    "E--",    "F--",    "F#--",    "G--",    "G#--",    "A-",     "A#-",     "B-",
    "C-",     "C#-",     "D-",     "D#-",     "E-",     "F-",     "F#-",     "G-",     "G#-",     "A",      "A#",      "B",
    "C",      "C#",      "D",      "D#",      "E",      "F",      "F#",      "G",      "G#",      "A+",     "A#+",     "B+",
    "C+",     "C#+",     "D+",     "D#+",     "E+",     "F+",     "F#+",     "G+",     "G#+",     "A++",    "A#++",    "B++",
    "C++",    "C#++",    "D++",    "D#++",    "E++",    "F++",    "F#++",    "G++",    "G#++",    "A+++",   "A#+++",   "B+++",
    "C+++",   "C#+++",   "D+++",   "D#+++",   "E+++",   "F+++",   "F#+++",   "G+++",   "G#+++",   "A++++",  "A#++++",  "B++++",
    "C++++",  "C#++++",  "D++++",  "D#++++",  "E++++",  "F++++",  "F#++++",  "G++++",  "G#++++",  "A+++++", "A#+++++", "B+++++",
    "C+++++", "C#+++++", "D+++++", "D#+++++", "E+++++", "F+++++", "F#+++++", "G+++++",
    "0x80", "0x81", "0x82", "0x83","0x84", "0x85", "0x86", "0x87","0x88", "0x89", "0x8A", "0x8B","0x8C", "0x8D", "0x8E", "0x8F",
    "0x90", "0x91", "0x92", "0x93","0x94", "0x95", "0x96", "0x97","0x98", "0x99", "0x9A", "0x9B","0x9C", "0x9D", "0x9E", "0x9F",
    "0xA0", "0xA1", "0xA2", "0xA3","0xA4", "0xA5", "0xA6", "0xA7","0xA8", "0xA9", "0xAA", "0xAB","0xAC", "0xAD", "0xAE", "0xAF",
    "0xB0", "0xB1", "0xB2", "0xB3","0xB4", "0xB5", "0xB6", "0xB7","0xB8", "0xB9", "0xBA", "0xBB","0xBC", "0xBD", "0xBE", "0xBF",
    "0xC0", "0xC1", "0xC2", "0xC3","0xC4", "0xC5", "0xC6", "0xC7","0xC8", "0xC9", "0xCA", "0xCB","0xCC", "0xCD", "0xCE", "0xCF",
    "0xD0", "0xD1", "0xD2", "0xD3","0xD4", "0xD5", "0xD6", "0xD7","0xD8", "0xD9", "0xDA", "0xDB","0xDC", "0xDD", "0xDE", "0xDF",
    "0xE0", "0xE1", "0xE2", "0xE3","0xE4", "0xE5", "0xE6", "0xE7","0xE8", "0xE9", "0xEA", "0xEB","0xEC", "0xED", "0xEE", "0xEF",
    "0xF0", "0xF1", "0xF2", "0xF3","0xF4", "0xF5", "0xF6", "0xF7","0xF8", "0xF9", "0xFA", "0xFB","0xFC", "0xFD", "0xFE", "0xFF"
};

#define MINNOTE 27          // minimum note music.pa will play (D#---)

#define NMUSTRK 4           // max number of notes at a time music.pa will play

typedef struct Note {
    uint32_t len;           // 32nd notes remaining
    uint8_t key;            // zero: rest; else: index into midkeystr[] and muskeystr[]
} Note;

typedef struct Tick {       // 32nd note
    Note notes[NMUSTRK];
} Tick;

static int allocticks, usedticks;
static Tick *ticks;

static uint32_t chunklen;
static uint32_t midi32;      // midi time for a 32nd note

static uint8_t readuint8 ();
static uint16_t readuint16 ();
static uint32_t readuint32 ();
static uint32_t readvaruint ();
static void addnote (uint32_t noteonat, uint32_t noteoffat, uint8_t key);
static char *lenstr (uint32_t len);

int main (int argc, char **argv)
{
    char *p;
    int verbose = 0;
    uint32_t trackenables = 0;
    midi32 = 120;
    uint32_t qtrpermin = 60;
    for (int i = 0; ++ i < argc;) {
        if (strcasecmp (argv[i], "-midi32") == 0) {
            if ((++ i >= argc) && (argv[i][0] == '-')) {
                fprintf (stderr, "count missing for -midi32\n");
                return 1;
            }
            midi32 = strtoul (argv[i], NULL, 0);
            continue;
        }
        if (strcasecmp (argv[i], "-qpm") == 0) {
            if ((++ i >= argc) && (argv[i][0] == '-')) {
                fprintf (stderr, "count missing for -qpm\n");
                return 1;
            }
            qtrpermin = strtoul (argv[i], NULL, 0);
            continue;
        }
        if (strcasecmp (argv[i], "-verbose") == 0) {
            verbose = 1;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        int n = strtol (argv[i], &p, 0);
        if ((*p != 0) || (n < 1) || (n > 31)) {
            fprintf (stderr, "track enable %s must be integer 1..31\n", argv[i]);
            return 1;
        }
        trackenables |= 1U << n;
    }
    if (trackenables == 0) trackenables = 0xFFFFFFFFU;

    allocticks = 1000;
    ticks = malloc (allocticks * sizeof *ticks);
    if (ticks == NULL) abort ();

    uint16_t ntrks = 1;
    uint16_t trkno = 0;
    while (ntrks != 0) {
        chunklen = 8;
        uint32_t chunktype = readuint32 ();
        chunklen = readuint32 ();
        if ((chunktype == FOURCHARS ("MThd")) && (chunklen == 6)) {
            uint16_t format = readuint16 ();
                     ntrks  = readuint16 ();
            uint16_t division = readuint16 ();
            if (verbose) printf ("\nMThd format=%u ntrks=%u division=%04X\n", format, ntrks, division);
        }
        else if (chunktype == FOURCHARS ("MTrk")) {
            if (verbose) printf ("\nMTrk len=%u\n", chunklen);
            uint32_t abstime = 0;
            uint32_t linenum = 0;
            uint8_t lastatus = 0;
            uint32_t chanonats[16];
            memset (chanonats, 0, sizeof chanonats);
            ++ trkno;
            if (verbose) printf (" trkno=%d\n", trkno);
            while (chunklen > 0) {
                uint32_t deltatime = readvaruint ();
                abstime += deltatime;
                if (verbose) printf ("%010u.%06u.%06u dt=%06u", abstime, trkno, ++ linenum, deltatime);
                uint8_t status = readuint8 ();
                if (status & 0x80) {
                    lastatus = status;
                } else {
                    chunklen ++;
                    ungetc (status, stdin);
                    status = lastatus;
                }
                uint8_t chan = status & 15;
                switch (status >> 4) {
                    case 8: {
                        uint8_t key = readuint8 ();
                        uint8_t vel = readuint8 ();
                        if (verbose) printf (" noteoff  chan=%u key=%-6s vel=%u\n", chan, midkeystr[key], vel);
                        if (trackenables & (1U << trkno)) addnote (chanonats[chan], abstime, key);
                        break;
                    }
                    case 9: {
                        uint8_t key = readuint8 ();
                        uint8_t vel = readuint8 ();
                        if (verbose) printf (" noteon   chan=%u key=%-6s vel=%u\n", chan, midkeystr[key], vel);
                        if (vel == 0) {
                            if (trackenables & (1U << trkno)) addnote (chanonats[chan], abstime, key);
                        } else {
                            chanonats[chan] = abstime;
                        }
                        break;
                    }
                    case 10: {
                        uint8_t key = readuint8 ();
                        uint8_t vel = readuint8 ();
                        if (verbose) printf (" keypress chan=%u key=%-6s vel=%u\n", chan, midkeystr[key], vel);
                        break;
                    }
                    case 11: {
                        uint8_t ctlno = readuint8 ();
                        uint8_t value = readuint8 ();
                        if (verbose) printf (" ctlchange chan=%u ctlno=%u value=%u\n", chan, ctlno, value);
                        break;
                    }
                    case 12: {
                        uint8_t prgno = readuint8 ();
                        if (verbose) printf (" pgmchange chan=%u prgno=%u\n", chan, prgno);
                        break;
                    }
                    case 13: {
                        uint8_t press = readuint8 ();
                        if (verbose) printf (" chanpress chan=%u press=%u\n", chan, press);
                        break;
                    }
                    case 14: {
                        uint8_t lsbs = readuint8 ();
                        uint8_t msbs = readuint8 ();
                        if (verbose) printf (" pitchwheel chan=%u pitchg=%04X\n", chan, (msbs << 7) | lsbs);
                        break;
                    }
                    case 15: {
                        switch (chan) {
                            case 0:
                            case 7: {
                                if (verbose) printf (" sysex %02X :", status);
                                uint8_t e;
                                while (1) {
                                    e = readuint8 ();
                                    if (e & 0x80) break;
                                    if (verbose) printf (" %02X", e);
                                    if ((e >= 0x20) && (e <= 0x7E)) { if (verbose) printf ("<%c>", e); }
                                }
                                if (verbose) printf (" : %02X\n", e);
                                break;
                            }
                            case 2: {
                                uint8_t lsb = readuint8 ();
                                uint8_t msb = readuint8 ();
                                if (verbose) printf (" songposition value=%u\n", (msb << 7) | lsb);
                                break;
                            }
                            case 3: {
                                uint8_t song = readuint8 ();
                                if (verbose) printf (" songselect song=%u\n", song);
                                break;
                            }
                            case 6: {
                                if (verbose) printf (" tunerequest\n");
                                break;
                            }
                            case 8: {
                                if (verbose) printf (" timingclock\n");
                                break;
                            }
                            case 10: {
                                if (verbose) printf (" startplaying\n");
                                break;
                            }
                            case 11: {
                                if (verbose) printf (" continue\n");
                                break;
                            }
                            case 12: {
                                if (verbose) printf (" stoplaying\n");
                                break;
                            }
                            case 14: {
                                if (verbose) printf (" activesensing\n");
                                break;
                            }

                            // metaevent
                            case 15: {
                                uint8_t type = readuint8 ();
                                uint8_t len  = readuint8 ();
                                uint8_t buf[len+1];
                                if (verbose) printf (" metaevent type=%u len=%u ", type, len);
                                for (uint8_t i = 0; i < len; i ++) {
                                    uint8_t e = readuint8 ();
                                    if (verbose) printf (" %02X", e);
                                    if ((e < 0x20) || (e > 0x7E)) e = '.';
                                    buf[i] = e;
                                }
                                buf[len] = 0;
                                if (verbose) printf (" <%s>\n", buf);
                                break;
                            }
                            default: {
                                if (verbose) printf (" undefined status=%02X\n", status);
                                abort ();
                            }
                        }
                    }
                }
            }
            -- ntrks;
        }
        else {
            if (verbose) printf ("\nignoring %u byte chunk %c%c%c%c\n", chunklen, (chunktype >> 24) & 0xFF,
                    (chunktype >> 16) & 0xFF, (chunktype >> 8) & 0xFF, chunktype & 0xFF);
            while (chunklen > 0) { getc (stdin); -- chunklen; }
        }
    }

    if (verbose) {
        printf ("\nARRAY\n\n");
        for (int t = 0; t < usedticks; t ++) {
            printf ("%08d:", t);
            Tick *tick = &ticks[t];
            for (int i = 0; i < NMUSTRK; i ++) {
                uint8_t key = tick->notes[i].key;
                if (key != 0) {
                    printf (" %-6s", midkeystr[key]);
                }
            }
            printf ("\n");
        }
    }

    // write out MU file
    printf ("\nC=%u\n", qtrpermin);
    for (int t = 0; t < usedticks;) {
        Tick *tick = &ticks[t];

        if (t % 32 == 0) {
            printf ("Y=%d\n", t / 32 + 1);
        }

        // see if any notes at this tick
        // save length of shortest note
        uint32_t len = 0;
        for (int i = 0; i < NMUSTRK; i ++) {
            if (tick->notes[i].key != 0) {
                if ((len == 0) || (len > tick->notes[i].len)) {
                    len = tick->notes[i].len;
                }
            }
        }

        // if any note found, print shortest length and all notes on the line
        // then skip that shortest length
        if (len > 0) {
            uint16_t bits[16];
            memset (bits, 0, sizeof bits);
            for (int i = 0; i < NMUSTRK; i ++) {
                uint8_t key = tick->notes[i].key;
                bits[key/16] |= 1U << (key & 15);
            }

            printf ("%s (", lenstr (len));
            int first = 1;
            for (int i = 0; ++ i < 256;) {
                if (bits[i/16] & (1U << (i & 15))) {
                    if (! first) printf (", ");
                    printf ("%s", muskeystr[i]);
                    first = 0;
                }
            }
            printf (")\n");
        }

        // no notes found, output a rest for however many consecutive rest ticks
        else {
            while (++ len < usedticks - t) {
                ++ tick;
                for (int i = 0; i < NMUSTRK; i ++) {
                    if (tick->notes[i].key != 0) goto endrest;
                }
            }
        endrest:;
            printf ("%s R\n", lenstr (len));
        }
        t += len;
    }

    // five seconds of rest at the end
    for (int i = 0; i < 5; i ++) {
        printf ("%s R\n", lenstr (qtrpermin * 8 / 60));
    }

    printf ("$\n");
    return 0;
}

// read 8 bits from midi file
static uint8_t readuint8 ()
{
    if (chunklen == 0) {
        fprintf (stderr, "end-of-chunk reached\n");
        abort ();
    }
    -- chunklen;
    int ch = getc (stdin);
    if (ch < 0) {
        fprintf (stderr, "end-of-file reached\n");
        abort ();
    }
    return ch;
}

// read 16 bits from midi file
static uint16_t readuint16 ()
{
    uint16_t value = 0;
    for (int i = 0; i < 2; i ++) {
        uint8_t c = readuint8 ();
        value = (value << 8) | c;
    }
    return value;
}

// read 32 bits from midi file
static uint32_t readuint32 ()
{
    uint32_t value = 0;
    for (int i = 0; i < 4; i ++) {
        uint8_t c = readuint8 ();
        value = (value << 8) | c;
    }
    return value;
}

// read variable integer bits from midi file
static uint32_t readvaruint ()
{
    uint32_t value = readuint8 ();
    if (value & 0x80) {
        value &= 0x7F;
        uint8_t c;
        do {
            c = readuint8 ();
            value = (value << 7) + (c & 0x7F);
        } while (c & 0x80);
    }
    return value;
}

// add a note to the MUSIC.PA output array
//  input:
//   noteonat  = abstime note turned on at
//   noteoffat = abstime note turned off at
//   key       = the note
//  output:
//   note written to ticks[] array if there is room
static void addnote (uint32_t noteonat, uint32_t noteoffat, uint8_t key)
{
    // if note too low, move it up an octave
    while (key < MINNOTE) key += 12;

    // convert abstimes to 32nd note ticks
    uint32_t ontick  = (noteonat  + midi32 / 2) / midi32;
    uint32_t offtick = (noteoffat + midi32 / 2) / midi32;
    if (offtick > ontick) {

        // make sure array is big enough
        if (allocticks < offtick) {
            allocticks = offtick * 3 / 2;
            ticks = realloc (ticks, allocticks * sizeof *ticks);
            if (ticks == NULL) abort ();
        }

        // keep track of how many entries actually used
        if (usedticks < offtick) {
            memset (&ticks[usedticks], 0, (offtick - usedticks) * sizeof *ticks);
            usedticks = offtick;
        }

        // use one of 4 MU channels for the note
        for (int i = 0; i < NMUSTRK; i ++) {

            // see if channel unused for duration of note
            for (int t = ontick; t < offtick; t ++) {
                if (ticks[t].notes[i].key != 0) goto busy;
            }

            // fill in note to the channel
            for (int t = ontick; t < offtick; t ++) {
                ticks[t].notes[i].key = key;
                ticks[t].notes[i].len = offtick - t;
            }
            return;
        busy:;
        }

        // no empty track, toss note
    }
}

// return MUSIC.PA length code string for the given length
//  input:
//   len = length in ticks (32nd note)
static char *lenstr (uint32_t len)
{
    static char lenbuf[256];
    char *p = lenbuf;

    if ((len / 32) * 2 + 10 > sizeof lenbuf) {
        fprintf (stderr, "len %u too long\n", len);
        exit (1);
    }

    // whole notes
    while (len >= 32) {
        if (p > lenbuf) *(p ++) = 'T';
        *(p ++) = 'B';
        len -= 32;
    }

    // half notes
    if (len & 16) {
        if (p > lenbuf) *(p ++) = 'T';
        *(p ++) = 'M';
    }

    // quarter notes
    if (len & 8) {
        if (p > lenbuf) *(p ++) = 'T';
        *(p ++) = 'C';
    }

    // 8th notes
    if (len & 4) {
        if (p > lenbuf) *(p ++) = 'T';
        *(p ++) = 'Q';
    }

    // 16th notes
    if (len & 2) {
        if (p > lenbuf) *(p ++) = 'T';
        *(p ++) = 'S';
    }

    // 32nd notes
    if (len & 1) {
        if (p > lenbuf) *(p ++) = 'T';
        *(p ++) = 'D';
    }

    *p = 0;
    return lenbuf;
}
