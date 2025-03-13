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

// tcl scripting main

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <tcl.h>
#include <unistd.h>

#include "readprompt.h"
#include "tclmain.h"

#define ABORT() do { fprintf (stderr, "abort() %s:%d\n", __FILE__, __LINE__); abort (); } while (0)

struct TclExit {
    TclExit *next;
    Tcl_Interp *interp;
    char command[1];
};

// internal TCL commands
static Tcl_ObjCmdProc cmd_atexit;
static Tcl_ObjCmdProc cmd_ctrlcflag;
static Tcl_ObjCmdProc cmd_help;

bool volatile ctrlcflag;
static bool logflushed;
static char *inihelp;
static TclFunDef const *fundefs;
static int logfd, pipefds[2], oldstdoutfd;
static pthread_cond_t logflcond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t logflmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tclxmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t logtid;
static sigset_t sigintmask;
static TclExit *tclexits;

static void tclloop (char const *progname, Tcl_Interp *interp);
static void siginthand (int signum);
static void *logthread (void *dummy);
static void closelog ();
static int writepipe (int fd, char const *buf, int len);
static void tclatexit ();
static void getexedir (char *buf, int buflen);



int tclmain (
    TclFunDef const *tclfundefs,    // table of program-specific tcl functions
    char *argv0,                    // argv[0]
    char const *progname,           // program name for prompt and error messages
    char const *logname,            // log file name or NULL
    char const *scriptini,          // script init filename or NULL
    int scriptargc,                 // script filename or 0
    char const *const *scriptargv,  // script filename or NULL
    bool repeat)                    // repeated prompts from tty
{
    Tcl_FindExecutable (argv0);

    Tcl_Interp *interp = Tcl_CreateInterp ();
    if (interp == NULL) ABORT ();
    int rc = Tcl_Init (interp);
    if (rc != TCL_OK) {
        char const *err = Tcl_GetStringResult (interp);
        if ((err != NULL) && (err[0] != 0)) {
            fprintf (stderr, "%s: error %d initialing tcl: %s\n", progname, rc, err);
        } else {
            fprintf (stderr, "%s: error %d initialing tcl\n", progname, rc);
        }
        ABORT ();
    }

    fundefs = tclfundefs;
    for (int i = 0; tclfundefs[i].name != NULL; i ++) {
        if (Tcl_CreateObjCommand (interp, tclfundefs[i].name, tclfundefs[i].func, NULL, NULL) == NULL) ABORT ();
    }
    if (Tcl_CreateObjCommand (interp, "atexit", cmd_atexit, NULL, NULL) == NULL) ABORT ();
    if (Tcl_CreateObjCommand (interp, "ctrlcflag", cmd_ctrlcflag, NULL, NULL) == NULL) ABORT ();
    if (Tcl_CreateObjCommand (interp, "help", cmd_help, NULL, NULL) == NULL) ABORT ();

    char exedir[1024];
    getexedir (exedir, sizeof exedir);
    if (Tcl_SetVar (interp, "Z8LHOME", exedir, 0) == NULL) ABORT ();

    // redirect stdout if -log given
    if (logname != NULL) {
        fflush (stdout);
        fflush (stderr);

        // open log file
        logfd = open (logname, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (logfd < 0) {
            fprintf (stderr, "error creating %s: %m\n", logname);
            return 1;
        }

        // save old stdout so we can fclose stdout then fclose it
        oldstdoutfd = dup (STDOUT_FILENO);
        if (oldstdoutfd < 0) ABORT ();

        // create pipe that will be used for new stdout then open stdout on it
        if (pipe (pipefds) < 0) ABORT ();
        fclose (stdout);  // fclose() after pipe() so pipefds[0] doesn't get STDOUT_FILENO
        if (dup2 (pipefds[1], STDOUT_FILENO) < 0) ABORT ();
        close (pipefds[1]);
        pipefds[1] = -1;
        stdout = fdopen (STDOUT_FILENO, "w");
        if (stdout == NULL) ABORT ();
        setlinebuf (stdout);

        // logfd = log file
        // stdout = write end of pipe
        // oldstdout = original stdout (probably a tty)
        // pipefds[0] = read end of pipe

        // create thread to copy from pipe to both old stdout and log file
        if (pthread_create (&logtid, NULL, logthread, NULL) < 0) ABORT ();
        pthread_detach (logtid);
        atexit (closelog);

        // \377 means only write to log file, not to old stdout
        printf ("\377*S*T*A*R*T*U*P*\n");
    }

    // set ctrlcflag on control-C, but exit if two in a row
    signal (SIGINT, siginthand);

    // maybe there is a script init file
    if (scriptini != NULL) {
        rc = Tcl_EvalFile (interp, scriptini);
        if (rc != TCL_OK) {
            char const *err = Tcl_GetStringResult (interp);
            if ((err == NULL) || (err[0] == 0)) fprintf (stderr, "%s: error %d evaluating scriptini %s\n", progname, rc, scriptini);
                                  else fprintf (stderr, "%s: error %d evaluating scriptini %s: %s\n", progname, rc, scriptini, err);
            Tcl_EvalEx (interp, "puts $::errorInfo", -1, TCL_EVAL_GLOBAL);
            return 1;
        }
        char const *res = Tcl_GetStringResult (interp);
        if ((res != NULL) && (res[0] != 0)) inihelp = strdup (res);
    }

    // if given a filename, process that file as a whole
    if (scriptargc > 0) {

        // get tcl filename from first argument
        char const *scriptfn = scriptargv[0];

        // pass remaining arguments as list 'argv'
        Tcl_Obj *scriptargobjs[scriptargc-1];
        for (int i = 0; ++ i < scriptargc;) {
            scriptargobjs[i-1] = Tcl_NewStringObj (scriptargv[i], -1);
        }
        if (Tcl_ObjSetVar2 (interp, Tcl_NewStringObj ("argv", 4), NULL, Tcl_NewListObj (scriptargc - 1, scriptargobjs), TCL_GLOBAL_ONLY) == NULL) {
            ABORT ();
        }

        // evaluate the script file as a whole
        rc = Tcl_EvalFile (interp, scriptfn);
        if (rc != TCL_OK) {
            char const *res = Tcl_GetStringResult (interp);
            if ((res == NULL) || (res[0] == 0)) fprintf (stderr, "%s: error %d evaluating script %s\n", progname, rc, scriptfn);
                                  else fprintf (stderr, "%s: error %d evaluating script %s: %s\n", progname, rc, scriptfn, res);
            Tcl_EvalEx (interp, "puts $::errorInfo", -1, TCL_EVAL_GLOBAL);
            return 1;
        }
    }

    // otherwise, prompt and process commands from stdin
    else if (repeat && (isatty (STDIN_FILENO) > 0)) {
        printf ("\nTCL scripting, do 'help' for %s-specific commands\n", progname);
        printf ("  do 'exit' to exit %s altogether\n", progname);
        puts ("  do ctrl-D to close prompt but keep running");
        puts ("  do ctrl-C to re-open prompt");

        while (true) {
            tclloop (progname, interp);
            sigset_t oldsigmask;
            if (sigprocmask (SIG_BLOCK, &sigintmask, &oldsigmask) != 0) ABORT ();
            while (! ctrlcflag) {
                sigsuspend (&oldsigmask);
                if (errno != EINTR) ABORT ();
            }
            if (sigprocmask (SIG_UNBLOCK, &oldsigmask, NULL) != 0) ABORT ();
        }
    } else {
        if (isatty (STDIN_FILENO) > 0) {
            printf ("\nTCL scripting, do 'help' for %s-specific commands\n", progname);
            printf ("  do 'exit' to exit %s\n", progname);
        }
        tclloop (progname, interp);
    }

    tclatexit ();
    Tcl_Finalize ();

    return 0;
}

// loop processing TCL commands read from stdin until eof
static void tclloop (char const *progname, Tcl_Interp *interp)
{
    char prompt[strlen(progname)+4];
    sprintf (prompt, "%s> ", progname);
    for (char const *line;;) {
        ctrlcflag = false;
        line = readprompt (prompt);
        ctrlcflag = false;
        if (line == NULL) break;

        int rc = Tcl_EvalEx (interp, line, -1, TCL_EVAL_GLOBAL);

        char const *res = Tcl_GetStringResult (interp);
        int rel = (res == NULL) ? 0 : strlen (res);
        char rer[rel+1];
        memcpy (rer, res, rel);
        rer[rel] = 0;

        char const *eri = Tcl_GetVar (interp, "::errorInfo", TCL_GLOBAL_ONLY);
        int erl = (eri == NULL) ? 0 : strlen (eri);
        char err[erl+1];
        memcpy (err, eri, erl);
        err[erl] = 0;

        Tcl_EvalEx (interp, "flush stdout", -1, TCL_EVAL_GLOBAL);
        Tcl_EvalEx (interp, "flush stderr", -1, TCL_EVAL_GLOBAL);

        if (rc != TCL_OK) {
            if (rer[0] == 0) fprintf (stderr, "%s: error %d evaluating command\n", progname, rc);
               else fprintf (stderr, "%s: error %d evaluating command: %s\n", progname, rc, rer);
            if (err[0] != 0) fprintf (stderr, "%s\n", err);
        }
        else if (rer[0] != 0) puts (rer);
    }
}

static void siginthand (int signum)
{
    if ((isatty (STDIN_FILENO) > 0) && (write (STDIN_FILENO, "\n", 1) > 0)) { }
    if (logfd > 0) writepipe (STDOUT_FILENO, "\377^C\n", 4);
    if (ctrlcflag) exit (1);
    ctrlcflag = true;
}

// read from pipefds[0] and write to both logfd and oldstdoutfd
// need this instead of 'tee' command cuz control-C aborts 'tee'
static void *logthread (void *dummy)
{
    bool atbol = true;
    bool echoit = true;
    char buff[4096];
    int rc;
    while ((rc = read (pipefds[0], buff, sizeof buff)) > 0) {

        int ofs = 0;
        int len;
        while ((len = rc - ofs) > 0) {

            // \377 means write following line only to log file, don't echo to old stdout
            char *p = (char *) memchr (buff + ofs, 0377, len);
            if (p != NULL) len = p - buff - ofs;
            if (len == 0) {
                echoit = false;
                ofs ++;
                continue;
            }

            // but \377\000 means set logflushed flag
            if (! echoit && (buff[ofs] == 0)) {
                logflushed = true;
                if (pthread_cond_broadcast (&logflcond) != 0) ABORT ();
                echoit = true;
                ofs ++;
                continue;
            }

            // chop amount being written to one line so we can prefix next line with timestamp
            char *q = (char *) memchr (buff + ofs, '\n', len);
            if (q != NULL) len = ++ q - buff - ofs;

            // maybe echo to old stdout
            if (echoit && (writepipe (oldstdoutfd, buff + ofs, len) < 0)) {
                fprintf (stderr, "logthread: error writing to stdout: %m\n");
                ABORT ();
            }

            // always wtite to log file with timestamp
            if (atbol) {
                struct timeval nowtv;
                if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
                struct tm nowtm = *localtime (&nowtv.tv_sec);
                dprintf (logfd, "%02d:%02d:%02d.%06d ", nowtm.tm_hour, nowtm.tm_min, nowtm.tm_sec, (int) nowtv.tv_usec);
            }
            if (writepipe (logfd, buff + ofs, len) < 0) {
                fprintf (stderr, "logthread: error writing to logfile: %m\n");
                ABORT ();
            }

            // if just reached eol, re-enable stdout echoing and remember to timestamp next line
            ofs    += len;
            atbol   = (buff[ofs-1] == '\n');
            echoit |= atbol;
        }
    }
    close (logfd);
    return NULL;
}

// make sure all written by calling thread has been written to original stdout
// ie, not sitting in logthread's pipe
void logflush ()
{
    if (logfd > 0) {
        if (pthread_mutex_lock (&logflmutex) != 0) ABORT ();
        logflushed = false;
        if (writepipe (STDOUT_FILENO, "\377", 2) < 0) ABORT ();
        while (! logflushed) {
            if (pthread_cond_wait (&logflcond, &logflmutex) != 0) ABORT ();
        }
        if (pthread_mutex_unlock (&logflmutex) != 0) ABORT ();
    }
}

// make sure log file gets last bit written to stdout
static void closelog ()
{
    fclose (stdout);
    stdout = NULL;
    pthread_join (logtid, NULL);
    logtid = 0;
}

static int writepipe (int fd, char const *buf, int len)
{
    while (len > 0) {
        int rc = write (fd, buf, len);
        if (len <= 0) {
            if (len == 0) errno = EPIPE;
            return -1;
        }
        buf += rc;
        len -= rc;
    }
    return 0;
}

// execute the given command when the program exits
int cmd_atexit (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    switch (objc) {
        case 2: {
            char const *opstr = Tcl_GetString (objv[1]);
            if (strcasecmp (opstr, "help") == 0) {
                puts ("");
                puts ("  atexit \"<command>\" - execute command on exit");
                puts ("");
                return TCL_OK;
            }
            int oplen = strlen (opstr);
            TclExit *tclx = (TclExit *) malloc (oplen + sizeof *tclx);
            tclx->interp = interp;
            memcpy (tclx->command, opstr, ++ oplen);
            if (pthread_mutex_lock (&tclxmutex) != 0) ABORT ();
            tclx->next = tclexits;
            tclexits = tclx;
            if (pthread_mutex_unlock (&tclxmutex) != 0) ABORT ();
            if (tclx->next == NULL) atexit (tclatexit);
            return TCL_OK;
        }
    }
    Tcl_SetResult (interp, (char *) "bad number of arguments", TCL_STATIC);
    return TCL_ERROR;
}

static void tclatexit ()
{
    if (pthread_mutex_lock (&tclxmutex) != 0) ABORT ();
    for (TclExit *tclx; (tclx = tclexits) != NULL;) {
        tclexits = tclx->next;
        if (pthread_mutex_unlock (&tclxmutex) != 0) ABORT ();
        Tcl_EvalEx (tclx->interp, tclx->command, -1, TCL_EVAL_GLOBAL);
        free (tclx);
        if (pthread_mutex_lock (&tclxmutex) != 0) ABORT ();
    }
    if (pthread_mutex_unlock (&tclxmutex) != 0) ABORT ();
}

// read and clear the control-C flag
int cmd_ctrlcflag (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    bool oldctrlcflag = ctrlcflag;
    switch (objc) {
        case 2: {
            char const *opstr = Tcl_GetString (objv[1]);
            if (strcasecmp (opstr, "help") == 0) {
                puts ("");
                puts ("  ctrlcflag - read control-C flag");
                puts ("  ctrlcflag <boolean> - read control-C flag and set to given value");
                puts ("");
                puts ("  in any case, control-C flag is cleared at command prompt");
                puts ("");
                return TCL_OK;
            }
            int temp;
            int rc = Tcl_GetBooleanFromObj (interp, objv[1], &temp);
            if (rc != TCL_OK) return rc;
            if (sigprocmask (SIG_BLOCK, &sigintmask, NULL) != 0) ABORT ();
            oldctrlcflag = ctrlcflag;
            ctrlcflag = temp != 0;
            if (sigprocmask (SIG_UNBLOCK, &sigintmask, NULL) != 0) ABORT ();
            // fallthrough
        }
        case 1: {
            Tcl_SetObjResult (interp, Tcl_NewIntObj (oldctrlcflag));
            return TCL_OK;
        }
    }
    Tcl_SetResult (interp, (char *) "bad number of arguments", TCL_STATIC);
    return TCL_ERROR;
}

// print help messages
int cmd_help (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    puts ("");
    bool didatexit = false;
    bool didctrlc  = false;
    for (TclFunDef const *fd = fundefs; fd->help != NULL; fd ++) {
        if (! didatexit && (strcasecmp (fd->name, "atexitflag") > 0)) {
            printf ("  %10s - %s\n", "atexit", "execute given command on exit");
            didatexit = true;
        }
        if (! didctrlc && (strcasecmp (fd->name, "ctrlcflag") > 0)) {
            printf ("  %10s - %s\n", "ctrlcflag", "read and clear control-C flag");
            didctrlc = true;
        }
        printf ("  %10s - %s\n", fd->name, fd->help);
    }
    if (! didatexit) {
        printf ("  %10s - %s\n", "atexit", "execute given command on exit");
    }
    if (! didctrlc) {
        printf ("  %10s - %s\n", "ctrlcflag", "read and clear control-C flag");
    }
    puts ("");
    puts ("for help on specific command, do '<command> help'");
    if (inihelp != NULL) {
        puts ("");
        puts (inihelp);
    }
    puts ("");
    return TCL_OK;
}

/////////////////
//  Utilities  //
/////////////////

// set result to formatted string
void Tcl_SetResultF (Tcl_Interp *interp, char const *fmt, ...)
{
    char *buf = NULL;
    va_list ap;
    va_start (ap, fmt);
    if (vasprintf (&buf, fmt, ap) < 0) ABORT ();
    va_end (ap);
    Tcl_SetResult (interp, buf, (void (*) (char *)) free);
}

// get executeable directory into buf excluding trailing /
static void getexedir (char *buf, int buflen)
{
    int rc = readlink ("/proc/self/exe", buf, buflen);
    if (rc < 0) {
        fprintf (stderr, "getexedir: error readink link /proc/self/exe: %m\n");
        ABORT ();
    }
    if (rc >= buflen) {
        fprintf (stderr, "getexedir: link /proc/self/exe longer than %d bytes\n", buflen - 1);
        ABORT ();
    }
    buf[rc] = 0;

    char *lastslash = strrchr (buf, '/');
    if (lastslash == NULL) {
        fprintf (stderr, "getexedir: no slash in executable name %s\n", buf);
        ABORT ();
    }
    *lastslash = 0;
}
