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

// VC8 small computer handbook PDP-8/I, 1970, p87

#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <X11/Xlib.h>

#include "z8ldefs.h"
#include "z8lutil.h"

#define MAXBUFFPTS 32768    // size of pdp8lvc8.v videoram
#define DEFPERSISTMS 500    // how long non-storage points last
#define MINPERSISTMS 10     // ...smaller and the up/down buttons won't work
#define MAXPERSISTMS 60000  // ...larger and it will overflow 16-bit counter

#define XYSIZE 1024
#define DEFWINSIZE XYSIZE
#define MINWINSIZE 64
#define MAXWINSIZE XYSIZE
#define BORDERWIDTH 2

#define EF_DN 04000     // done executing last command
#define EF_WT 00040     // de-focus beam for writing
#define EF_ST 00020     // enable storage mode
#define EF_ER 00010     // erase storage screen (450ms)
#define EF_CO 00004     // 1=red; 0=green
#define EF_CH 00002     // display select
#define EF_IE 00001     // interrupt enable

#define EF_MASK 077

enum VC8Type {
    VC8TypeN = 0,
    VC8TypeE = 1,
    VC8TypeI = 2
};

struct VC8Pt {
    uint16_t x;
    uint16_t y;
    uint16_t t;
};

struct Button {
    Button ();
    virtual ~Button ();

    virtual void draw (unsigned long color) = 0;
    bool clicked ();

protected:
    bool lastpressed;
    int xleft, xrite, ytop, ybot;
};

struct ClearButton : Button {
    ClearButton ();
    void position ();
    virtual void draw (unsigned long color);
};

struct TriButton : Button {
    TriButton (bool upbut);
    void position ();
    virtual void draw (unsigned long color);

private:
    bool upbut;
    XSegment segs[3];
};

struct PersisButton {
    TriButton upbutton;
    TriButton dnbutton;

    PersisButton ();
    void setpms ();
    void draw (unsigned long color);

private:
    char pmsbuf[12];
    int pmslen;
};

static bool mintimes;
static int winsize;
static unsigned long xwin;
static _XDisplay *xdis;
static _XGC *xgc;
static int indices[MAXWINSIZE*MAXWINSIZE];
static uint16_t pms;
static uint16_t timemss[XYSIZE*XYSIZE];
static unsigned long blackpixel;
static unsigned long foreground;
static unsigned long graylevels[6];
static unsigned long whitepixel;
static VC8Pt allpoints[XYSIZE*XYSIZE];
static VC8Type vc8type;

static void thread ();
static void drawpt (int i, bool erase);
static int mappedxy (VC8Pt const *pt);
static void setfg (unsigned long color);
static int xioerror (Display *xdis);

int main (int argc, char **argv)
{
    pms     = DEFPERSISTMS;
    vc8type = VC8TypeN;
    winsize = DEFWINSIZE;

    char const *iodevname = NULL;

    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Access VC8/E or VC8/I display");
            puts ("");
            puts ("  ./z8lvc8 [-pms <persistence-milliseconds>] [-size <windowsize-pixels>] e | i");
            printf ("     persistence %d..%d, default %dmS\n", MINPERSISTMS, MAXPERSISTMS, DEFPERSISTMS);
            printf ("     windowsize %d..%d, default %d pixels\n", MINWINSIZE, MAXWINSIZE, DEFWINSIZE);
            puts ("     e : process VC8/E-style IO instructions");
            puts ("     i : process VC8/I-style IO instructions");
            puts ("");
            return 0;
        }

        // maybe override default ephemeral persistence
        if (strcasecmp (argv[i], "-pms") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "-pms missing time argument\n");
                return 1;
            }
            int intpms = atoi (argv[i]);
            if (intpms < MINPERSISTMS) intpms = MINPERSISTMS;
            if (intpms > MAXPERSISTMS) intpms = MAXPERSISTMS;
            pms = intpms;
            continue;
        }

        // maybe override default window size
        if (strcasecmp (argv[i], "-size") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "-size missing size argument\n");
                return 1;
            }
            winsize = atoi (argv[i]);
            if (winsize < MINWINSIZE) winsize = MINWINSIZE;
            if (winsize > MAXWINSIZE) winsize = MAXWINSIZE;
            continue;
        }

        // decode display type
        if (strcasecmp (argv[i], "e") == 0) {
            iodevname = "vc8e";
            vc8type = VC8TypeE;
            continue;
        }
        if (strcasecmp (argv[i], "i") == 0) {
            iodevname = "vc8i";
            vc8type = VC8TypeI;
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }
    if (vc8type == VC8TypeN) {
        fprintf (stderr, "specify display type with 'e' or 'i'\n");
        return 1;
    }
    fprintf (stderr, "main: device type %s defined\n", iodevname);

    thread ();
    return 0;
}

static void thread ()
{
    // access pdp8lvc8.v registers
    Z8LPage z8p;
    uint32_t volatile *vcat = z8p.findev ("VC", NULL, NULL, true);
    switch (vc8type) {
        case VC8TypeE: {
            vcat[2] = VC2_TYPEE;
            break;
        }
        case VC8TypeI: {
            vcat[2] = 0;
            break;
        }
        default: ABORT ();
    }

    // open connection to XServer
    xdis = XOpenDisplay (NULL);
    if (xdis == NULL) {
        fprintf (stderr, "thread: error opening X display\n");
        ABORT ();
    }

    // set up handler to be called when window closed
    XSetIOErrorHandler (xioerror);

    // create and display a window
    blackpixel = XBlackPixel (xdis, 0);
    whitepixel = XWhitePixel (xdis, 0);
    Window xrootwin = XDefaultRootWindow (xdis);
    xwin = XCreateSimpleWindow (xdis, xrootwin, 100, 100,
        winsize + BORDERWIDTH * 2, winsize + BORDERWIDTH * 2, 0, whitepixel, blackpixel);
    XMapRaised (xdis, xwin);

    char const *typname;
    switch (vc8type) {
        case VC8TypeE: typname = "VC8/E"; break;
        case VC8TypeI: typname = "VC8/I"; break;
        default: ABORT ();
    }
    char hostname[256];
    gethostname (hostname, sizeof hostname);
    hostname[255] = 0;
    char windowname[8+256+12];
    snprintf (windowname, sizeof windowname, "%s (%s:%d)", typname, hostname, (int) getpid ());
    XStoreName (xdis, xwin, windowname);

    // set up to draw on the window
    XGCValues gcvalues;
    memset (&gcvalues, 0, sizeof gcvalues);
    gcvalues.foreground = whitepixel;
    gcvalues.background = blackpixel;
    xgc = XCreateGC (xdis, xwin, GCForeground | GCBackground, &gcvalues);

    // set up gray levels for the intensities
    Colormap cmap = XDefaultColormap (xdis, 0);
    for (int i = 0; i < 4; i ++) {
        XColor xcolor;
        memset (&xcolor, 0, sizeof xcolor);
        xcolor.red   = (i + 1) * 65535 / 4;
        xcolor.green = (i + 1) * 65535 / 4;
        xcolor.blue  = (i + 1) * 65535 / 4;
        xcolor.flags = DoRed | DoGreen | DoBlue;
        XAllocColor (xdis, cmap, &xcolor);
        graylevels[i] = xcolor.pixel;
    }
    unsigned long graypixel = graylevels[2];

    // set up red and green colors
    unsigned long greenpix, magenpix, redpixel;
    {
        XColor xcolor;
        memset (&xcolor, 0, sizeof xcolor);
        xcolor.red   = 65535;
        xcolor.flags = DoRed | DoGreen | DoBlue;
        XAllocColor (xdis, cmap, &xcolor);
        redpixel = xcolor.pixel;

        xcolor.blue  = 65535;
        XAllocColor (xdis, cmap, &xcolor);
        magenpix = xcolor.pixel;

        xcolor.red   = 0;
        xcolor.blue  = 0;
        xcolor.green = 65535;
        XAllocColor (xdis, cmap, &xcolor);
        greenpix = xcolor.pixel;
    }
    if (vc8type == VC8TypeE) {
        graylevels[0] = graylevels[1] = redpixel;
        graylevels[2] = graylevels[3] = greenpix;
    }

    // no points being displayed
    memset (indices, -1, sizeof indices);

    // buttons
    ClearButton clearbutton;
    PersisButton persisbutton;
    persisbutton.setpms ();

    // repeat until display closed
    char wsizbuf[12] = { 0 };
    uint16_t wsiztim = 0;
    uint32_t lastsec = 0;
    uint32_t counter = 0;
    int allins = 0;
    int allrem = 0;
    uint16_t oldeflags = 0;
    foreground = whitepixel;
    while (true) {

        // wait for points queued or reset
        // max of time for next persistance expiration
        usleep (50000);

        // copy points from processor
        struct timeval nowtv;
        if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
        uint16_t timems = ((nowtv.tv_sec * 1000000ULL) + nowtv.tv_usec) / 1000;
        int allnew = allins;
        while (true) {
            vcat[3] = VC3_DEQUEUE;                      // get point queued by processor
            uint32_t pt;
            for (int i = 0; (pt = vcat[4]) & VC4_READBUSY; i ++) {
                if (i > 1000000) {
                    fprintf (stderr, "thread: pdp8lvc8.v is stuck\n");
                    ABORT ();
                }
            }
            if (pt & VC4_EMPTY) break;
            VC8Pt p;
            p.x = (pt & VC4_XCOORD) / VC4_XCOORD0;
            p.y = (pt & VC4_YCOORD) / VC4_YCOORD0;
            p.t = (pt & VC4_TCOORD) / VC4_TCOORD0;
            int mxy = mappedxy (&p);                    // this pixel is occupied by this point now
            indices[mxy] = allins;
            int xy = p.y * XYSIZE + p.x;                // this is latest occurrence of the x,y
            timemss[xy] = timems;                       // remember when we got it
            allpoints[allins] = p;                      // copy to end of all points ring
            allins = (allins + 1) % MAXBUFFPTS;
            if (allrem == allins) {
                allrem = (allrem + 1) % MAXBUFFPTS;     // ring overflowed, remove oldest point
            }
            counter ++;
        }

        uint16_t neweflags = (vcat[2] & VC2_EFLAGS) / VC2_EFLAGS0;

        // allpoints:  ...  allrem  ...  allnew  ...  allins  ...
        //                  <old points> <new points>

        // maybe erase everything
        if (~ oldeflags & neweflags & EF_ER) {
            XClearWindow (xdis, xwin);
            XFlush (xdis);
            usleep (450000);
            allins = allnew = allrem = 0;
            vcat[1]  = 0;                               // writing VC1_INSERT=0, VC1_REMOVE=0
            vcat[2] |= EF_DN * VC2_EFLAGS0;
        }

        // get window size
        XWindowAttributes winattrs;
        if (XGetWindowAttributes (xdis, xwin, &winattrs) == 0) ABORT ();
        int newwinxsize = winattrs.width  - BORDERWIDTH * 2;
        int newwinysize = winattrs.height - BORDERWIDTH * 2;
        int newwinsize  = (newwinxsize < newwinysize) ? newwinxsize : newwinysize;
        if (newwinsize < MINWINSIZE) newwinsize = MINWINSIZE;
        if (newwinsize > MAXWINSIZE) newwinsize = MAXWINSIZE;

        // if window size changed, clear window then re-draw old points at new scaling
        if (winsize != newwinsize) {
            winsize = newwinsize;
            XClearWindow (xdis, xwin);

            // refill indices to indicate where the latest point can be found
            memset (indices, -1, MAXWINSIZE * MAXWINSIZE * sizeof *indices);
            for (int refill = allrem; refill != allins; refill = (refill + 1) % (XYSIZE * XYSIZE)) {
                int mxy = mappedxy (&allpoints[refill]);
                indices[mxy] = refill;
            }

            // redraw the oldest points
            for (int redraw = allrem; redraw != allnew; redraw = (redraw + 1) % (XYSIZE * XYSIZE)) {
                drawpt (redraw, false);
            }

            // set new positions of buttons
            clearbutton.position ();
            persisbutton.upbutton.position ();
            persisbutton.dnbutton.position ();

            // set up to display new window size
            sprintf (wsizbuf, "%d", winsize);
            wsiztim = timems;
        }

        // ephemeral mode: erase old points what have timed out
        if (! (neweflags & EF_ST)) {
            while (allrem != allnew) {

                // get next point, break out if no more left
                VC8Pt *p = &allpoints[allrem];

                // see if it has timed out or has been superceded
                int xy = p->y * XYSIZE + p->x;
                int mxy = mappedxy (p);
                uint16_t dtms = timems - timemss[xy];
                if ((indices[mxy] == allrem) && (dtms < pms)) break;

                // draw in black and remove if so
                drawpt (allrem, true);

                // remove timed-out entry from ring
                allrem = (allrem + 1) % (XYSIZE * XYSIZE);
            }
        }

        // draw new points passed to us from processor
        while (allnew != allins) {
            drawpt (allnew, false);
            allnew = (allnew + 1) % (XYSIZE * XYSIZE);
        }

        // draw border box
        setfg (graypixel);
        for (int i = 0; i < BORDERWIDTH; i ++) {
            XDrawRectangle (xdis, xwin, xgc, i, i, winsize + BORDERWIDTH * 2 - 1 - i * 2, winsize + BORDERWIDTH * 2 - 1 - i * 2);
        }

        // storage mode: draw CLEAR button
        // ephemeral mode: draw persistance UP/DOWN buttons
        if (neweflags & EF_ST) {
            if (! (oldeflags & EF_ST)) {
                persisbutton.draw (blackpixel);
            }
            clearbutton.draw (magenpix);
        } else {
            if (oldeflags & EF_ST) {
                clearbutton.draw (blackpixel);
            }
            persisbutton.draw (magenpix);
        }

        // maybe show new window size
        if (wsizbuf[0] != 0) {
            bool dead = (timems - wsiztim > 1000);
            setfg (dead ? blackpixel : magenpix);
            XDrawString (xdis, xwin, xgc, BORDERWIDTH * 2, BORDERWIDTH * 2 + 10, wsizbuf, strlen (wsizbuf));
            if (dead) wsizbuf[0] = 0;
        }

        XFlush (xdis);

        // maybe print counts once per minute
        if (mintimes) {
            uint32_t thissec = nowtv.tv_sec;
            if (lastsec / 60 != thissec / 60) {
                if (lastsec != 0) {
                    uint32_t pps = counter / (thissec - lastsec);
                    if (pps != 0) fprintf (stderr, "thread: %u point%s per second\n", pps, (pps == 1 ? "" : "s"));
                }
                lastsec = thissec;
                counter = 0;
            }
        }

        // storage mode: check for clear button clicked
        if (neweflags & EF_ST) {
            if (clearbutton.clicked ()) XClearWindow (xdis, xwin);
        } else {
            int16_t delta = 0;
            if (persisbutton.upbutton.clicked ()) {
                delta += pms / 4;
            }
            if (persisbutton.dnbutton.clicked ()) {
                delta -= pms / 4;
            }
            if (delta != 0) {
                persisbutton.draw (blackpixel);
                pms += delta;
                if (pms < MINPERSISTMS) pms = MINPERSISTMS;
                if (pms > MAXPERSISTMS) pms = MAXPERSISTMS;
                persisbutton.setpms ();
            }
        }

        oldeflags = neweflags;
    }
}

static void drawpt (int i, bool erase)
{
    // don't bother drawing if superceded
    VC8Pt *pt = &allpoints[i];
    int mxy = mappedxy (pt);
    if (indices[mxy] == i) {

        // latest of this xy, draw point
        setfg (erase ? blackpixel : graylevels[pt->t]);
        int mx = pt->x * winsize / XYSIZE;
        int my = pt->y * winsize / XYSIZE;
        XDrawPoint (xdis, xwin, xgc, mx + BORDERWIDTH, my + BORDERWIDTH);

        // if erasing, point no longer in allpoints
        if (erase) indices[mxy] = -1;
    }
}

static int mappedxy (VC8Pt const *pt)
{
    int mx = pt->x * winsize / XYSIZE;
    int my = pt->y * winsize / XYSIZE;
    return my * MAXWINSIZE + mx;
}

static void setfg (unsigned long color)
{
    if (foreground != color) {
        foreground = color;
        XSetForeground (xdis, xgc, foreground);
    }
}

// gets called when window closed
// if we don't provide this, sometimes get segfaults when closing
static int xioerror (Display *xdis)
{
    fprintf (stderr, "xioerror: X server IO error\n");
    exit (1);
}

/////////////////////////////
//  Common to all Buttons  //
/////////////////////////////

Button::Button ()
{
    lastpressed = false;
}

Button::~Button ()
{ }

bool Button::clicked ()
{
    Window root, child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;
    Bool ok = XQueryPointer (xdis, xwin, &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);
    if (ok && (win_x > xleft) && (win_x < xrite) && (win_y > ytop) && (win_y < ybot)) {
        // mouse inside button area, if pressed, remember that
        if (mask & Button1Mask) {
            lastpressed = true;
        } else if (lastpressed) {
            // mouse button released in button area after being pressed in button area
            lastpressed = false;
            return true;
        }
    } else {
        // mouse outside button area, pretend button was released
        lastpressed = false;
    }
    return false;
}

/////////////////////////////////////////
//  Circle Button for Clearing Screen  //
/////////////////////////////////////////

#define BUTTONDIAM 32

ClearButton::ClearButton ()
{
    position ();
}

void ClearButton::position ()
{
    xleft = winsize + BORDERWIDTH - BUTTONDIAM;
    xrite = winsize + BORDERWIDTH;
    ytop  = winsize + BORDERWIDTH - BUTTONDIAM;
    ybot  = winsize + BORDERWIDTH;
}

void ClearButton::draw (unsigned long color)
{
    setfg (color);
    XDrawArc (xdis, xwin, xgc, winsize + BORDERWIDTH - BUTTONDIAM, winsize + BORDERWIDTH - BUTTONDIAM, BUTTONDIAM - 1, BUTTONDIAM - 1, 0, 360 * 64);
    XDrawString (xdis, xwin, xgc, winsize + BORDERWIDTH + 1 - BUTTONDIAM, winsize + BORDERWIDTH + 4 - BUTTONDIAM / 2, "CLEAR", 5);
}

#undef BUTTONDIAM

////////////////////////////////////////////////
//  Triangle Button for Altering Persistence  //
////////////////////////////////////////////////

#define BUTTONDIAM 48

TriButton::TriButton (bool upbut)
{
    this->upbut = upbut;
    position ();
}

void TriButton::position ()
{
    xleft = winsize + BORDERWIDTH - BUTTONDIAM - 00;
    xrite = winsize + BORDERWIDTH - 00;
    if (upbut) {
        ytop = winsize + BORDERWIDTH - BUTTONDIAM     - BUTTONDIAM / 2 - 00;
        ybot = winsize + BORDERWIDTH - BUTTONDIAM / 2 - BUTTONDIAM / 2 - 00;
        segs[0].x1 = winsize + BORDERWIDTH - BUTTONDIAM - 00;
        segs[0].y1 = winsize + BORDERWIDTH - BUTTONDIAM / 2 - BUTTONDIAM / 2 - 00;
        segs[0].x2 = winsize + BORDERWIDTH - 1 - 00;
        segs[0].y2 = winsize + BORDERWIDTH - BUTTONDIAM / 2 - BUTTONDIAM / 2 - 00;
        segs[1].x1 = winsize + BORDERWIDTH - 1 - 00;
        segs[1].y1 = winsize + BORDERWIDTH - BUTTONDIAM / 2 - BUTTONDIAM / 2 - 00;
        segs[1].x2 = winsize + BORDERWIDTH - BUTTONDIAM / 2 - 00;
        segs[1].y2 = winsize + BORDERWIDTH - BUTTONDIAM     - BUTTONDIAM / 2 - 00;
        segs[2].x1 = winsize + BORDERWIDTH - BUTTONDIAM / 2 - 00;
        segs[2].y1 = winsize + BORDERWIDTH - BUTTONDIAM     - BUTTONDIAM / 2 - 00;
        segs[2].x2 = winsize + BORDERWIDTH - BUTTONDIAM - 00;
        segs[2].y2 = winsize + BORDERWIDTH - BUTTONDIAM / 2 - BUTTONDIAM / 2 - 00;
    } else {
        ytop = winsize + BORDERWIDTH - BUTTONDIAM / 2 - 00;
        ybot = winsize + BORDERWIDTH - 00;
        segs[0].x1 = winsize + BORDERWIDTH - BUTTONDIAM - 00;
        segs[0].y1 = winsize + BORDERWIDTH - BUTTONDIAM / 2 - 00;
        segs[0].x2 = winsize + BORDERWIDTH - 1 - 00;
        segs[0].y2 = winsize + BORDERWIDTH - BUTTONDIAM / 2 - 00;
        segs[1].x1 = winsize + BORDERWIDTH - 1 - 00;
        segs[1].y1 = winsize + BORDERWIDTH - BUTTONDIAM / 2 - 00;
        segs[1].x2 = winsize + BORDERWIDTH - BUTTONDIAM / 2 - 00;
        segs[1].y2 = winsize + BORDERWIDTH - 1 - 00;
        segs[2].x1 = winsize + BORDERWIDTH - BUTTONDIAM / 2 - 00;
        segs[2].y1 = winsize + BORDERWIDTH - 1 - 00;
        segs[2].x2 = winsize + BORDERWIDTH - BUTTONDIAM - 00;
        segs[2].y2 = winsize + BORDERWIDTH - BUTTONDIAM / 2 - 00;
    }
}

void TriButton::draw (unsigned long color)
{
    setfg (color);
    XDrawSegments (xdis, xwin, xgc, segs, 3);
}

//////////////////////////
//  Persistance Button  //
//////////////////////////

PersisButton::PersisButton ()
    : upbutton (true),
      dnbutton (false)
{
    pmslen = 0;
}

void PersisButton::setpms ()
{
    pmslen = snprintf (pmsbuf, sizeof pmsbuf, "%u ms", pms);
}

void PersisButton::draw (unsigned long color)
{
    upbutton.draw (color);
    dnbutton.draw (color);
    XDrawString (xdis, xwin, xgc, winsize + BORDERWIDTH + 1 - BUTTONDIAM, winsize + BORDERWIDTH + 4 - BUTTONDIAM * 3 / 4, pmsbuf, pmslen);
}

#undef BUTTONDIAM
