#include "code.h"
#include <VG/openvg.h>
#include <VG/vgu.h>
#include <EGL/egl.h>
#include <bcm_host.h>
#include <assert.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <string.h>
#include "drawtext.h"
#include <time.h>
#include <sys/time.h>
#include "token.h"
#include <png.h>
#include "lspng.h" // uses token
#include "mousedriver.h"

#include "lf.h"
#include "lfmap.h"

#define drawshell main

#define RAMDIR          "/V:"
#define CODESIZE	8
#define TIMESIZE	25
#define NAMESIZE        128
// ^ should be same as in users.c
/*
struct termios newterm, savedterm;
VD saveterm() {
  tcgetattr(fileno(stdin), &savedterm);
}
VD rawterm() {
  memcpy(&newterm, &savedterm, sizeof(struct termios));
  newterm.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
  newterm.c_cc[VTIME] = 0;
  newterm.c_cc[VMIN] = 0;
  tcsetattr(fileno(stdin), TCSANOW, &newterm);
}
VD restoreterm() {
  tcsetattr(fileno(stdin), TCSANOW, &savedterm);
Gs("term restored\n");
}
*/
//VD interrupthandler() {
//  restoreterm();
//  _exit(0);
//}
CS spin(CS spinstr) {
  IF (spinstr[0] EQ '|')  { spinstr[0] = '/';  }
  EF (spinstr[0] EQ '/')  { spinstr[0] = '-';  }
  EF (spinstr[0] EQ '-')  { spinstr[0] = '\\'; }
  EF (spinstr[0] EQ '\\') { spinstr[0] = '|';  }
  RT spinstr;
}  

IN runsizecmd(CCS cmd, INP width, INP height) {
  // consider variable argument alternative - cmd, delimeter, p, p, p, p
  FS cmdf = OPENCMD(cmd);
  CH fmode = AW;
  IN gwidth = 0;
  IN gheight = 0;
  IN inch = NUL;
  WI NOTFEOF(cmdf) {
    inch = GETFCH(cmdf);
    IF (fmode EQ AW AND CHISINT(inch)) {
      gwidth = (gwidth * 10) + (inch - A0);
    } EF(fmode EQ AH AND CHISINT(inch)) {
      gheight = (gheight * 10) + (inch - A0);
    } EF (inch EQ Ax) {
      IF (fmode EQ AW) { fmode = AH; } // consider more dimensions
    } EL { fmode = AX; BK; }
  }
  CLOSECMD(cmdf);
  *width = gwidth;
  *height = gheight;
  RT 0; // should return command status
}

typedef struct _TextboxList {
  FS file;
  LF top, left, bottom, right, width, height;
  IN pxwidth, pxheight, pxlength;
  IN dtcols, dtrows;
//  PF data;
 /* PF = Pointer to structure with ->LF list, P to next, P to prev .. dynamic length! */
  CS pixels;
//  IN pvwidth, pvheight, pvlength;
//  CS pvpixels, pvdata;
  struct _TextboxList *parent;
  struct _TextboxList *next;
} TextboxList;
IN addtextbox(TextboxList **tblist, FS file) {
  TextboxList *tb = (TextboxList *)malloc(sizeof(TextboxList));
  IF (!tb) { Rxc(97); RT 0; } // malloc fail
  tb->file = file;
  IF (file EQNUL) { Yc('!'); } // file not open
  tb->next = *tblist; // 
  tb->parent = tb->next; // <--- each new textbox is a child of the previous one by default for now
  *tblist = tb;       // <------- new boxes added to start of list
  RT 1; // file added
}

typedef struct _ItemBox {
  List *itemlist;
  IN selindex;
  CS fontpath;
  IN itemfontsize;
  IN selfontsize;
  IN itemsperrow;
  IN numitems;
  XY boxpos;
  XY boxmargin;
  IN numlines;
  XYWH itembox;
  XY selboxoffset;
  XY selboxpos;
  XY selboxmargin;
  XYWH selbox;
  RGB itemboxbg;
  RGB selboxbg;
  RGB selitemtext;
  XYWH itemselbox;
  RGB itemselboxbg;
  RGB itemboxtext;
  struct _ItemBox *next;
} ItemBox;
ItemBox *createitembox(List *itemlist, IN selindex, CS fontpath, IN itemfontsize, IN selfontsize, IN itemsperrow, IN numitems, IN x, IN y) {
  ItemBox *ib = (ItemBox *)MEM(sizeof(ItemBox));
  ib->itemlist = itemlist;
  ib->selindex = selindex;
  ib->fontpath = fontpath;
  ib->itemfontsize = itemfontsize;
  ib->selfontsize = selfontsize;
  ib->itemsperrow = itemsperrow;
  ib->numitems = numitems;
  ib->boxpos.x = x;
  ib->boxpos.y = y;
  ib->boxmargin.x = 8;
  ib->boxmargin.y = 8;
  ib->numlines = (ib->numitems / ib->itemsperrow) + 1;
  ib->selboxoffset.x = 0;
  ib->selboxoffset.y = 64;
  ib->selboxpos.x = ib->boxpos.x + ib->selboxoffset.x; // offset for itemselbox ........
  ib->selboxpos.y = ib->boxpos.y + ib->selboxoffset.y;
  ib->selboxmargin.x = 8;
  ib->selboxmargin.y = 8;
  ib->selbox.x = ib->boxpos.x - ib->selboxoffset.x;        // duplicate of selboxpos ? nope..
  ib->selbox.y = ib->boxpos.y - ib->selboxoffset.y; // flip?
  ib->selbox.w = ib->selboxmargin.x + ib->selfontsize + ib->selboxmargin.x;
  ib->selbox.h = ib->selboxmargin.y + ib->selfontsize + ib->selboxmargin.y;
  ib->selboxbg = (RGB){ 0x40, 0x40, 0x40 };
  ib->selitemtext = (RGB){ 0xFF, 0xFF, 0x00 };
  ib->itemselbox.x = ib->selboxpos.x - 2 + (((ib->selindex - 1) % ib->itemsperrow) * ib->itemfontsize);
  ib->itemselbox.y = ib->selboxpos.y - 3 - (((ib->selindex - 1) / ib->itemsperrow) * ib->itemfontsize);
  ib->itemselbox.w = ib->itemfontsize + 2; // extra 2 on left ....
  ib->itemselbox.h = ib->itemfontsize + 2; // extra 2 at top ....
  ib->itemselboxbg = (RGB){ 0xFF, 0xFF, 0x00 };
  ib->itembox.x = ib->boxpos.x - ib->boxmargin.x;
  ib->itembox.w = ib->boxmargin.x + (ib->itemsperrow * ib->itemfontsize) + ib->boxmargin.x;
  ib->itembox.h = ib->boxmargin.y + (ib->numlines * ib->itemfontsize) + ib->boxmargin.y;
  ib->itembox.h SUBS ((ib->numlines - 1) * ib->itemfontsize) / 2; // only one line should be full height
  ib->itembox.y = ib->boxpos.y - ib->itembox.h + ib->boxmargin.y; // - ((ib->numlines - 1) * ib->itemfontsize); // aligns with first line of text // CONSIDER VARIABLE FONT SIZE
  ib->itemboxbg = (RGB){ 0x30, 0x30, 0x30 };
  ib->itemboxtext = (RGB){ 0xFF, 0x00, 0xFF };
  RT ib;
}
ItemBox *allitemboxes = NULL;
VD additembox(ItemBox *ib) {
  IF (allitemboxes) {
    ItemBox *aib = allitemboxes;
    WI (aib->next) { aib = aib->next; }
    aib->next = ib;
  } EL { allitemboxes = ib; }
}

VD drawitembox(ItemBox *ib) {
// ^^^ itemboxy will be affected  yh this!!! itemboxh before itemboxy
  setfill(ib->itemboxbg.r, ib->itemboxbg.g, ib->itemboxbg.b, 1);
  drawroundrect(ib->itembox.x, ib->itembox.y, ib->itembox.w, ib->itembox.h, 32, 16);
  setfill(ib->selboxbg.r, ib->selboxbg.g, ib->selboxbg.b, 1);
  drawroundrect(ib->selbox.x, ib->selbox.y, ib->selbox.w, ib->selbox.h, 16, 32);
  List *selitem = ib->itemlist; // itemlist could be multichar...
  WI (selitem) {
    IF (selitem->index EQ ib->selindex) {
      setfill(ib->selitemtext.r, ib->selitemtext.g, ib->selitemtext.b, 1);
      drawchars(ib->fontpath, ib->selfontsize, ib->selboxpos.x, ib->selboxpos.y, selitem->item); // inc selx to draw several chars
// [ni hao ma] -> [aaaa bbbb cccc]
// need to draw selection before list so that selection boxes can also be drawn
// drawroundrect() 400, 500 + selx, sely
    }
    selitem = selitem->next;
  }
  setfill(ib->itemselboxbg.r, ib->itemselboxbg.g, ib->itemselboxbg.b, 1);
  drawroundrect(ib->itemselbox.x, ib->itemselbox.y, ib->itemfontsize, ib->itemfontsize, 4, 4);
  setfill(ib->itemboxtext.r, ib->itemboxtext.g, ib->itemboxtext.b, 1);
  drawlist(ib->fontpath, 32, ib->boxpos.x, ib->boxpos.y, ib->itemlist, ib->itemsperrow, ib->itemfontsize, ib->itemfontsize);
}
/*
VD signalhandler(IN signo) {
  Mc('-');
  Cs("SIG"); Yi(signo); 
  Mc(':');
  Cs(strsignal(signo));
  Mc('-'); _;
  restoreterm();
  _exit(0);
}

VD catchsignals() {
  struct sigaction sa;
  sa.sa_handler = signalhandler;
  IN si = 0;
  WI (INC si LQ 64) {
    IF (si EQ SIGCHLD) { CT; }
    sigaction(si, &sa, NULL);
  }
  // SIGTERM, SIGSEGV, SIGINT, SIGHUP, etc ... maybe skip some ..
//  signal(SIGTERM, interrupthandler);
//IF (errno) { eRi(errno); }
//  signal(SIGSEGV, interrupthandler);
//IF (errno) { eRi(errno); }
//  signal(SIGINT, interrupthandler);
//IF (errno) { eRi(errno); }
//  signal(SIGHUP, interrupthandler);
//IF (errno) { eRi(errno); }
}
*/
/*
typedef struct _LFList {
  LF value;
  struct _LFList *next;
} LFList;

typedef struct _Parameter {
  CS name;
  LFList values;
  struct _Parameter *subvalues;
  struct _Parameter *next;
} Parameter;
*/ // moved to lf.h
typedef struct _Context {
  IN index;
  CS id;
  CS name; // can maybe use a param instead
  CS label;
  CS fontpath;
//  IN fontsize;
  XYWH box;
  XYWH namebox;
  XYWH parambox;
  XYWH mapbox;
  XY curve;
  IN textsize;
  CS text;
  IN texti;
  CH visible;
  RGB colour;
  FP opacity;
  CH bdvisible;
  RGB bdcolour;
  FP bdwidth;
  FP bdopacity;
  CH bgvisible;
  RGB bgcolour;
  FP bgopacity;
  List *selectionlist;
  IN selectionindex;
  IN selnumitems;
  IN selitemsperrow;
  Parameter *parameters;
} Context;

typedef struct _Shell {
  CS fontpath;
  // -> multiple selections
  TextboxList *textfiles;
  //Textbox *userinputs;
  Box *stream;
  IN pagewidth, pageheight;
  IN gridwidth, gridheight;
  IN width, height;
  IN layer;
  UCH opacity;
//  FT_Library ftlib; maybe later
  Pointer pointer;
  Context context[10];
  Parameter *parameters;
  CH saveonchangetext;
  CH saveonchangespin;
  CH texthaschanged;
  CH spinhaschanged;
  CH spinstr[2];
  CH ctxti;
} Shell;
Shell shell;

IN drawshell($) {
  CHDIRORFAIL(RAMDIR) { RT1("CHDIRRAMDIR"); }
  FS ramfont = OPENCMD("intoram /font/ukai.ttf");
  CLOSECMD(ramfont);
  shell.fontpath = "/dev/shm/ukai.ttf";
//  atexit(restoreterm);
//  saveterm();
//  signal(SIGINT, signalhandler);
//  signal(SIGHUP, signalhandler);
//  signal(SIGTERM, signalhandler);
//  signal(SIGSEGV, signalhandler);
//  catchsignals();
//  rawterm();
//  IN pid = setsid();
//Ys("PID");
//Mi(pid);
  shell.pagewidth = 0;
  shell.pageheight = 0;
  runsizecmd("pagesize", &shell.pagewidth, &shell.pageheight);
  shell.gridwidth = 0;
  shell.gridheight = 0;
  runsizecmd("gridsize", &shell.gridwidth, &shell.gridheight);
  initSize(0, 0, shell.pagewidth, shell.pageheight);
  shell.layer = 0;
  shell.opacity = 200;
  init(shell.layer, &shell.width, &shell.height, shell.opacity);
  initglyphlibface(shell.fontpath);
  initpointer(&shell.pointer);
//  savescreenreader(NULL);
  // ------------------ FIRST FRAME -----------------------------------------------------------------
  start();
  setbackground(0, 0, 0, 1); // FULLBLACK    0, 0, 0, 1
  setfill(0xFF, 0xCC, 0x33, 1); // FULLFC3   0xFF, 0xCC, 0x33, 1
  drawchars(shell.fontpath, 160, 100, 100, "Connected...");
  end();
eYs("(stream");
  shell.stream = savescreenstream(0, 0, shell.width, shell.height);
eGs("init)");
  // ------------------------------------------------------------------------------------------------
  Parameter *spr = NULL;
  spr = lfaddparam(spr, "S1.WIDTH",   (LF){  shell.width, 0 }, "px");
  spr = lfaddparam(spr, "S1.HEIGHT",  (LF){ shell.height, 0 }, "px");
  spr = lfaddparam(spr, "S1.BOUNDL",  (LF){          720, 0 }, "degh");
  spr = lfaddparam(spr, "S1.BOUNDR",  (LF){         -720, 0 }, "degh");
  spr = lfaddparam(spr, "S1.BOUNDT",  (LF){          720, 0 }, "degv"); // or flip
  spr = lfaddparam(spr, "S1.BOUNDB",  (LF){         -720, 0 }, "degv");
  shell.parameters = spr;
// should draw to screen as debug for now
  shell.textfiles = NULL;
//  WI ($N GT 0) .. id:filename
//    { addtextbox(&textfiles, OPENFILE($1)); $$1; }
  // ------------------------------------------------------------------------------------------------
  FP ctxtheight = shell.height / 9.0f;
  CH ctxt = -1;
  WI (INC ctxt LQ 9) { // contexts 0-9 for now...
    Context *c = &shell.context[ctxt];
    c->index = ctxt;
//    c->name = " "; none because shell doesn't handle login
//    c->id = "overflow";
//    c->label = "OVERFLOW";
    c->fontpath = shell.fontpath;
//    c->fontsize = 64;
    // later: alignment
    IF (ctxt EQ 0) { c->box = (XYWH){ 100, 100, 0, 0 }; } // size will be updated by text draw functions
    EL { c->box = (XYWH){ 16, ((10 - ctxt) * ctxtheight), 0, 0 }; } // context 1-9 rows (probs 9-1)
    c->namebox = (XYWH){ 10, 10, 10, 10 };
// parambox
// mapbox
    c->curve = (XY){ 16, 16 }; // same as mouse
    c->textsize = 1024;
    c->text = CHMEM(c->textsize + 1);
    c->texti = 0;
    c->text[c->texti] = NUL;
    c->visible = 1;
    IF (ctxt EQ 0) { c->colour = (RGB){ 0x99,    0, 0    }; }
    EF (ctxt EQ 1) { c->colour = (RGB){ 0xFF,    0, 0    }; }
    EF (ctxt EQ 2) { c->colour = (RGB){ 0xFF, 0xFF, 0    }; }
    EF (ctxt EQ 3) { c->colour = (RGB){    0, 0xFF, 0    }; }
    EF (ctxt EQ 4) { c->colour = (RGB){    0, 0xFF, 0xFF }; }
    EF (ctxt EQ 5) { c->colour = (RGB){    0,    0, 0xFF }; }
    EF (ctxt EQ 6) { c->colour = (RGB){ 0xFF,    0, 0    }; }
    EF (ctxt EQ 7) { c->colour = (RGB){ 0xFF, 0xFF, 0xFF }; }
    EF (ctxt EQ 8) { c->colour = (RGB){ 0xAA, 0xAA, 0xAA }; }
    EF (ctxt EQ 9) { c->colour = (RGB){ 0x77, 0x77, 0x77 }; }
    c->opacity = 0.9;
    c->bdcolour = (RGB){ 0xFF, 0xCC, 0x33 };
    c->bdvisible = 1;
    c->bdopacity = 0.1f * ctxt;
    c->bgvisible = 1;
    c->bgcolour = c->colour;
//    c->bgcolour = (RGB){ 0x77, 0x77, 0x77 };
    c->bgopacity = 0.3;
    c->selectionlist = NULL;
    c->selectionindex = 0;
    c->selnumitems = 0;
    c->selitemsperrow = 16; // used by arrow keys
    IN centrex = shell.width / 2;
    IN centrey = shell.height / 2;
    IN smallw = 100;
    IN smallh = 100;
//    IN smalll = centrex - (smallw / 2);
//    IN smallt = centrey - 
    Parameter *cpr = NULL;
    cpr = lfaddparam(cpr,      "FONTSIZE",  (LF){           64, 0 }, NULL);
    cpr = lfaddparam(cpr, "LABELFONTSIZE",  (LF){           32, 0 }, NULL);
    cpr = lfaddparam(cpr, "POINTFONTSIZE",  (LF){           16, 0 }, NULL);
    cpr = lfaddparam(cpr,         "LABEL",  (LF){            0, 0 }, ">");
    cpr = lfaddparam(cpr,      "SCREEN.W",  (LF){  shell.width, 0 }, NULL);
    cpr = lfaddparam(cpr,      "SCREEN.H",  (LF){ shell.height, 0 }, NULL);
    cpr = lfaddparam(cpr,       "MOUSE.L",  (LF){ centrex - 50, 0 }, NULL);
    cpr = lfaddparam(cpr,       "MOUSE.T",  (LF){ centrey - 50, 0 }, NULL);
    cpr = lfaddparam(cpr,       "MOUSE.W",  (LF){          100, 0 }, NULL);
    cpr = lfaddparam(cpr,       "MOUSE.H",  (LF){          100, 0 }, NULL);
    cpr = lfaddparam(cpr,       "MOUSE.X",  (LF){            0, 0 }, NULL); // left ?
    cpr = lfaddparam(cpr,       "MOUSE.Y",  (LF){            0, 0 }, NULL); // top ?
    cpr = lfaddparam(cpr,       "M1.XFOCUS",   (LF){ 0, 0 }, "~");
    cpr = lfaddparam(cpr,       "M1.YFOCUS",   (LF){ 0, 0 }, "~");
    cpr = lfaddparam(cpr,       "M1.XOFFSET",  (LF){ 0, 0 }, "~");
    cpr = lfaddparam(cpr,       "M1.YOFFSET",  (LF){ 0, 0 }, "~");
    cpr = lfaddparam(cpr,       "M1.XVALUE",   (LF){ 0, 0 }, "~"); // need initial values
    cpr = lfaddparam(cpr,       "M1.YVALUE",   (LF){ 0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.OFFSETL",  (LF){ 0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.OFFSETR",  (LF){ 0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.OFFSETW",  (LF){ 0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.OFFSETT",  (LF){ 0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.OFFSETB",  (LF){ 0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.OFFSETH",  (LF){ 0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.PIXELL", (LF){ ctxt * 10, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.PIXELT", (LF){         0, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.PIXELW", (LF){        10, 0 }, "~"); // ..ignore them later
    cpr = lfaddparam(cpr,       "M1.PIXELH", (LF){        10, 0 }, "~"); // ..ignore them later
    c->parameters = cpr;
//lflistparams(c->parameters);
  }
  shell.saveonchangetext = 1;
  shell.saveonchangespin = 1;
  shell.texthaschanged = 0;
  shell.spinhaschanged = 0;
  shell.spinstr[0] = '-';
  shell.spinstr[1] = NUL;
  shell.ctxti = 0; // use overflow context by default
//  IN changes = 0;
//  IN everyframe = 1; // saves input EOF (spin) frames also
//  CH cmd = EOF; // should not become EOF in blocking mode
  IN ch = NUL; // non-blocking mode no longer accepts chars after EOF (no idea why)
  WI (1) {
//    ch = GETSTDCH;
    IF (ch NQ ':') // : could be due to CT and will not get stuck since it reads
      { ch = getchar(); }

//C2("ch=%d(%c)", ch, (ch GQ 32) ? ch : ' ');

    // \e1:VALUE=10:VALUE2=20:KEYS\n
    IF (ch EQ ':') { // SET PARAMETER (should have been preceded by \eCTXT) -------------------
      Context *c = &shell.context[shell.ctxti];
      // expect parameter
      CH parametername[NAMESIZE + 1];
      CS pname = parametername;
      WI (ch NQ '=') {
        ch = GETSTDCH;
        BKEQEOF(ch);
        IFEQ(ch, '=') { BK; }
        *pname = ch;
        INC pname;
        *pname = NUL;
      }
      IFEQ(ch, '=') { // this is a long float number
//Y1("%s=", parametername);
        LF lfvalue = lfreadfslf(stdin, &ch); // reads until ;, \n or any non-number char
        c->parameters = lfsetlfparam(c->parameters, parametername, lfvalue);
//Gs("=value");
//Mlf(lfvalue);
//Ylf(lfgetlfparam(c->parameters, parametername));
         // if parameter does not exist, addparam will be called by setparam
         // each call adds a new value to the list
      } EFEQ(ch, '|') { // this is a string of text
        CH strvalue[NAMESIZE + 1]; // username, code .... nothing longer than a variable name
        lfreadfscs(stdin, strvalue, &ch); // reads until ; or \n
        c->parameters = lfsetcsparam(c->parameters, parametername, strvalue);
      } EL { eRs(":param without =value or |string"); }
      IFEQ(ch, ';') { CT; } // check for another parameter / more input
      EFEQ(ch, ':') { CT; } // another parameter starts here ..
  //    EFEQ(ch, '\n') { } // input ends, time draw response frame
      EFNQ(ch, '\n')
        { eR2(":param=value ends in A%d (%c)", ch, ch); } // \e1:BLA=1;:X=2,1;:Z=1,2;KEYS\n
    }
    // ----------------------------------------------- END OF SET PARAMETER ---------
    // :Param=Value; handled above - CT each ; .... keys and \n (draw) below ...
    // ------------------------------------------------ TIME TO DRAW -------------
    IF (ch EQ '\n') { // buffer only clears on \n - use ; to delimit actions and text "\e5:FONTSIZE=32;hi"
//eCs("(drawing)");
      start();
      // ----------------------------------------- INFORMATION ------------------------------------------------------------------------      
      setbackground(0, 0, 0, 1.0);
      setfill(0x33, 0x33, 0x33, 1);
      drawchars(shell.fontpath, 32, shell.width - 700, shell.height - 128, "\xE9\xA7\x9D\xE9\xB9\xBF \xE9\xA7\x9D\xE9\xB9\xBF.");
      setfill(135, 206, 250, 1);
      drawchars(shell.fontpath, 20, 0, 0, "\xE6\x9B\xB2\xE7\xBA\xBF\xE6\x95\x91\xE5\x9B\xBD");
      setfill(0x00, 0xFF, 0x00, 1);
      drawcmd(shell.fontpath, 48, 0, shell.height - 48, "date", 0, 0);
      setfill(0xFF, 0xCC, 0x33, 1);
      drawcmd(shell.fontpath, 64, 900, shell.height - 64, "cat /proc/uptime", 0, 0);
      // consider stats for total textsize and total fontcache
      // ----------------------------------------- MOUSE -----------------------------------------------------------------------------
      Pointer *p = &shell.pointer;
      updatepointer(p);
      RGB mousebdc = { (p->mouseclick & LEFTCLICK  ) ? 0xFF : 0x77,
                       (p->mouseclick & MIDDLECLICK) ? 0xFF : 0x77,
                       (p->mouseclick & RIGHTCLICK ) ? 0xFF : 0x77 };
      FP mousebdw = 2.5f; // border width
      rgbaroundrect(p->mousebox, p->mousecurve, 1, mousebdc, 0.5, mousebdw, 0, (RGB){ 0, 0, 0 }, 0);
      
//      setfill(0x33, 0x33, 0x33, 1);
//      drawroundrect(shell->pointer->mousel, shell->pointer->mouseb, shell->pointer->mouser - shell->pointer->mousel,
//                                                                    shell->pointer->mouset - shell->pointer->mouseb, 16, 16);
//      setfill(red, 0x77, 0x77, 1);
//      drawroundrect(shell->pointer->mousel - 16.0f, shell->pointer->mouseb - 16.0f, 32.0f, 32.0f, 16, 16);
//      setfill(0x77, 0x77, blue, 1);
//      drawroundrect(shell->pointer->mouser - 16.0f, shell->pointer->mouset - 16.0f, 32.0f, 32.0f, 16, 16);
      // ------------------------------------------------------ SPIN ----------------------------------------------------------
      setfill(mousebdc.r, mousebdc.g, mousebdc.b, 1);
      drawchars(shell.fontpath, 128, shell.width - 128, shell.height - 128, spin(shell.spinstr));
      shell.spinhaschanged = 1;
      // ------------------------------------------------------ TEXT FILES ----------------------------------------------------------
//      TextboxList *tb = textfiles;
//      VGfloat y = 1000.0f;
//      WI (tb) {
//        setfill(0x33, 0xFF, 0xFF, 1); // use diff colours..
// get box position from tb later
//        y += drawfile(shell->fontpath, 20, 0, y, tb->file);
//        tb = tb->next;
//      }
      // ------------------------------------------------------ TEXT BUFFERS -------------------------------------------------------
      // draw input text in yellow, size 128: later use % of screen as guide
      IN ctxtj = -1;
      WI (INC ctxtj LQ 9) {
        Context *c = &shell.context[ctxtj];
        RGB bdcolour = c->bdcolour;
        IF (ctxtj EQ shell.ctxti)
          { bdcolour = (RGB){ 0xFF, 0xCC, 0x33 }; }
        IF (c->visible) { // should probably allow invisible-drawing to calculate size (no font loading)
          IN cfontsize = (lfgetlfparam(c->parameters, "FONTSIZE")).numb;
          IN clabelfontsize = (lfgetlfparam(c->parameters, "LABELFONTSIZE")).numb;
          IN cpointfontsize = (lfgetlfparam(c->parameters, "POINTFONTSIZE")).numb;
          CS clabel    = lfgetcsparam(c->parameters, "LABEL");
          IN cscreenw  = (lfgetlfparam(c->parameters, "SCREEN.W")).numb;
          IN cscreenh  = (lfgetlfparam(c->parameters, "SCREEN.H")).numb;
//          IN cpointfontsize = (lfgetlfparam(c->parameters, "POINTFONTSIZE")).numb;
          IN cmousex   = (lfgetlfparam(c->parameters, "MOUSE.X")).numb; // treat x as left to right (normal)
          IN cmousey   = (lfgetlfparam(c->parameters, "MOUSE.Y")).numb; // treat y as top to bottom (invert)
          IN cmousew   = (lfgetlfparam(c->parameters, "MOUSE.W")).numb; // box width
          IN cmouseh   = (lfgetlfparam(c->parameters, "MOUSE.H")).numb; // box height
          // draw text box
          XYWH cosbox = { c->box.x + cmousex, c->box.y - cmousey, 0, 0 }; // invert y, discard w/h (recalc)
          rgbatextlines(c->fontpath, cfontsize, c->text, &cosbox, 0, c->colour, 0.0f); // (visible=0) update box .w, .h only
          XYWH cbdbox = { cosbox.x - 3, cosbox.y - 3, cosbox.w + 6, cosbox.h + 6 };
          cbdbox.y SUBS cbdbox.h; // offset height due to vertical flip
          rgbaroundrect(cbdbox, c->curve, c->bdvisible, c->bdcolour, c->bdopacity, c->bdwidth, c->bgvisible, c->bgcolour, c->bgopacity);
          rgbatextlines(c->fontpath, cfontsize, c->text, &cosbox, c->visible, c->colour, c->opacity); // updates box .w, .h
          c->box.w = cosbox.w; c->box.h = cosbox.h; // updated by rgbatextlines (both times)
          // draw name box
          XYWH labelbox = { cbdbox.x, cbdbox.y + cbdbox.h, 0, 0 };
          rgbatextlines(c->fontpath, clabelfontsize, clabel, &labelbox, 0, c->colour, 0.0f); // update .w, .h
          XYWH lbdbox = { labelbox.x - 1, labelbox.y - 1, labelbox.w + 2, labelbox.h + 2 };
          lbdbox.y SUBS lbdbox.h; // vertical flip
          lbdbox.x SUBS lbdbox.w; // offset left
          rgbaroundrect(lbdbox, c->curve, c->bdvisible, c->bdcolour, c->bdopacity, c->bdwidth, c->bgvisible, c->bgcolour, c->bgopacity);
          rgbatextlines(c->fontpath, clabelfontsize, clabel, &labelbox, c->visible, c->colour, c->opacity); // updates .w, .h again
          // c->namebox.w = namebox.w; c->namebox.h = namebox.h
          // .... also draw param values
          XYWH mousebox = { cbdbox.x + cbdbox.w, cbdbox.y - cmouseh, cmousew, cmouseh };
          rgbaroundrect(mousebox, c->curve, c->bdvisible, c->bdcolour, c->bdopacity, c->bdwidth, c->bgvisible, c->bgcolour, c->bgopacity);
          IN mpointx = cmousex * cmousew / cscreenw;
          IN mpointy = cmousey * cmouseh / cscreenh;
          RGB mpointc = { 0x00, 0xFF, 0x00 };
          XYWH mpoint = { mousebox.x + mpointx, mousebox.y + cmouseh - mpointy, 0, 0 };
          CS mpointchar = "\xE2\x97\x86"; // filled diamond
//          rgbatextpoint(c->fontpath, cpointfontsize, mpointchar, &mpoint, 0, mpointc, 0.0f);
// ^ no need to calculate size .........................
          rgbatextpoint(c->fontpath, cpointfontsize, mpointchar, &mpoint, c->visible, mpointc, c->opacity);
// count how many mouse1 values exist
          // recalculate map pixel bounds before each user's is drawn
          lfupdatemapcoords(shell.parameters, c->parameters);
          IN m1pixell   = (lfgetlfparam(c->parameters, "M1.PIXELL")).numb;
          IN m1pixelt   = (lfgetlfparam(c->parameters, "M1.PIXELT")).numb;
          IN m1pixelw   = (lfgetlfparam(c->parameters, "M1.PIXELW")).numb;
          IN m1pixelh   = (lfgetlfparam(c->parameters, "M1.PIXELH")).numb;
          LF s1boundl   = lfgetlfparam( c->parameters, "S1.BOUNDL");
          LF s1boundt   = lfgetlfparam( c->parameters, "S1.BOUNDT");
          LF s1incremx  = lfgetlfparam( c->parameters, "S1.INCREMX");
          LF s1incremy  = lfgetlfparam( c->parameters, "S1.INCREMY");
          XYWH m1box = { m1pixell, m1pixelt, m1pixelw, m1pixelh };
          rgbaroundrect(m1box, c->curve, c->bdvisible, c->bdcolour, c->bdopacity, c->bdwidth, c->bgvisible, c->bgcolour, c->bgopacity);
          LFList *m1xvalues = lfgetparam(c->parameters, "M1.XVALUE"); // need XPIXEL XPIXEL ....
          LFList *m1yvalues = lfgetparam(c->parameters, "M1.YVALUE"); // need YPIXELS ... -S
          XYWH m1xlabel = { m1box.x,         m1box.y         , 200, clabelfontsize };
          XYWH m1ylabel = { m1box.x, m1box.y + clabelfontsize, 200, clabelfontsize };
          // ^ w and h ignored but could be used to pre-draw a background box
          RGB m1labelclr = { 0x00, 0xFF, 0xFF };
          RGB m1pointclr = { 0xFF, 0xFF, 0x00 };
          IN m1xtotal = lfvaluescount(m1xvalues);
          IN m1ytotal = lfvaluescount(m1yvalues);
          IF (m1xtotal NQ m1ytotal)
            { Rs("(x1 != y1)total"); }
          CH m1total[100];
          sprintf(m1total, "%d", m1xtotal);
          rgbatextpoint(c->fontpath, clabelfontsize, m1total, &m1xlabel, c->visible, m1labelclr, c->opacity);
          IN m1ix = -1;
          WI (INC m1ix LT m1xtotal) {
            LF m1valuex = lfvalueatindex(m1xvalues, m1ix);
            LF m1valuey = lfvalueatindex(m1yvalues, m1ix);
            LF m1pixelx = LFMUL(LFSUB(m1valuex, s1boundl), s1incremx);
            LF m1pixely = LFMUL(LFSUB(m1valuey, s1boundt), s1incremy);
            XYWH m1point = { m1pixelx.numb, m1pixely.numb, 0, 0 }; // no size, centre
            CS m1pointchar = "\xE2\x97\x86"; // filled diamond
            rgbatextpoint(c->fontpath, cpointfontsize, m1pointchar, &m1point, c->visible, m1pointclr, c->opacity);
          }
//lflistparams(c->parameters);
        }
      } // redraw on line break only rather than after every char - need linebreak to flush anyway - extra linebreaks won't trigger change
//        setfill(of->colour.r, of->colour.g, of->colour.b, of->opacity);
//        drawlines(shell.fontpath, 128, 300, 800, of->text);
      // ------------------------------------------------------ SELECTION BOXES -----------------------------------------------------        
      // each user sees differently ... need to save variants
      ctxtj = -1;
      WI (INC ctxtj LQ 9) {
        Context *c = &shell.context[ctxtj];
        IF (c->selectionlist) {
//        IN selnumitems = getlistsize(selectionlist);
//        IF (of->selecting EQ Ay) {
          ItemBox *ib = createitembox(c->selectionlist, c->selectionindex, shell.fontpath, 32, 128, c->selitemsperrow, c->selnumitems, 400, 500);
          drawitembox(ib);
          UNMEM(ib);
        }
      }
      end();
      // ------------------ FILE SAVE - NEEDS SELECTION BOX VARIANTS ----------------------------------------        
      IF ((shell.texthaschanged AND shell.saveonchangetext) OR (shell.spinhaschanged AND shell.saveonchangespin)) {
eCc('[');
//eYs("stream");
        updatescreenstream(shell.stream);
//eGs("update");
eCc(']');
//        IF (changes)
//          { updatescreenstream(stream); }
//        EL { update0screenstream(stream); }
//        savescreenshot(0, 0, width, height);
        shell.texthaschanged = 0;
        shell.spinhaschanged = 0;
      }
//Cc('@');
//      sched_yield();
//      SLEEP100MS; // 10 FPS FRAME LIMITER!!!!!!!! ENCODING WILL SLOW FURTHER
//      SLEEP10MS; // max 100fps-ish framerate
    } EL { // IF ch NQEOF ------------------------------------------------------------------
      IF (ch EQEOF) { eRs("(shell input eof)"); BK; } // WAIT500MS; CT; }
      // switching to normal terminal mode means no EOF is sent, but when used by popen, it seems to
      EF INRANGE(ch, ' ', '~') { eM1("%c", ch); }
      // EL { eR1("(%d)", ch); }
      // IF (ch IS ESC then read next char in case of context change \e1 \e2 \e3
      // input packets should (but don't have to) end with \e0 to reset context to overflow
      // \n\n\n\n should trigger spin change but no text change
      shell.texthaschanged = 1; // consider flagging for each context
      Context *c = &shell.context[shell.ctxti];
//Yi(ch); Mc('!');
      IF (ch EQ 2) {
        eRs("Ctrl+C");
//        signalhandler(SIGTERM);
        _exit(0);
      } EF (ch EQ 3) { // ctrl + C or ctrl + D
        eRs("Ctrl+D");
//        signalhandler(SIGTERM); // not actually SIGTERM, just emulating the code
        _exit(0);
  // later: interrupt savescreenid loop ...
//      } EF (ch EQ 9) { // TAB
//      } EF (ch EQ 10) { // NEWLINE
//      } EF (ch EQ 27) { // escape sequence ! perhaps remember it, eat it, apply the parameter instead ... handled below
      } EF (ch LT ' '
            AND ch NQ Aesc AND ch NQ Atab AND ch NQ Abackspace
            AND ch NQ Anewline AND ch NQ Acarriage) {
//      } EF (ch LT 32 AND ch NQ 9 AND ch NQ Anewline AND ch NQ Aesc) {
        eRs("interrupt");
        Yc('-'); Mi(ch); Yc('-');
        B1("ch=%d", ch);
        _exit(0);
//        signalhandler(SIGTERM);
      // ------------------- BACKSPACE --------------------------------------------------------------------------------
//      } EF (ch EQ 127) { // backspace (or is this delete ? not \b? 8?) 
      } EF (ch EQ Abackspace OR ch EQ 127) { // 8 from pipe, 127 from keyboard
        IF (c->texti GT 0) // prevent over-backspace
          { c->text[DEC c->texti] = NUL; }
      // ---------------- ESCAPE SEQUENCES ----------------------------------------------------------------------------
      } EF (ch EQ Aescape) { // 27) { // }  AND selecting EQ Ay) { // ESC ESC to cancel selection box
        IN ch2 = GETSTDCH; // expect [
        IF (ch2 EQ '[') {
          IN ch3 = GETSTDCH; // expect char
          IF (INRANGE(ch3, AA, AZ) OR INRANGE(ch3, Aa, Az)) {
            // all escape sequences end in a lEtTer ?? ?! check ...
            IF (ch3 EQ AA) {    // up
              c->selectionindex SUBS c->selitemsperrow;
              IF (c->selectionindex LT 1)
                { c->selectionindex ADDS c->selitemsperrow; }
            } EF (ch3 EQ AB) {  // down
              c->selectionindex ADDS c->selitemsperrow;
              IF (c->selectionindex GT c->selnumitems)
                { c->selectionindex SUBS c->selitemsperrow; }
            } EF (ch3 EQ AC) {  // right
              INC c->selectionindex;
              IF (c->selectionindex GT c->selnumitems)
                { DEC c->selectionindex; }
            } EF (ch3 EQ AD) {  // left
              DEC c->selectionindex;
              IF (c->selectionindex LT 1)
                { INC c->selectionindex; }
            } EL {
              eBc('('); eYs("\\e"); eRi(-1); eBc(')'); // unexpected escape sequence
            }
          } EL {
            WI (!(INRANGE(ch3, AA, AZ) OR INRANGE(ch3, Aa, Az))) {
              // should continue reading until a valid end char is seen \e[0;31m \e[A
              eMi(99); ch3 = GETSTDCH; // check docs ...............import key list
            }
          }
        // =========== SET CONTEXT ==============================
        } EF (ch2 EQ Aescape) { // \e\e
          shell.ctxti = 0; // same as \e0\e
        } EF INRANGE(ch2, A0, A9) {
          IF (ch2 EQ A0) { // \e0nnnn\e
            IN contexti = 0;
            IN chn = GETSTDCH;
            WI (chn NQ '\e') {
              IF INRANGE(chn, A0, A9) {
                contexti = (contexti * 10) + (chn - A0);
//G1("contexti = %d", contexti);
              } EL { eR1("bad chn (%d)", chn); }
            }
            shell.ctxti = contexti; // c updates next iteration
          } EL { 
//G1("contexti = %d", ch2 - A0);
            shell.ctxti = ch2 - A0;
          } // \e1 - \e9
        } EL { eMi(88); } // bad escape sequence
      // -------------------------------------------------------------------------------------
      // ============ CONFIRM SELECTION ON SPACE =================
      } EF (ch EQ ' ' AND c->selectionlist NQNULL) {
        IN termi = c->texti;
        WI (termi GQ 0 AND c->text[termi] NQ '[')
          { DEC termi; } // find last [term] (to replace with selection value)
        IF (termi GQ 0 AND c->selectionindex GQ 0) { // confirm
          CS tbuf = &c->text[termi];
          List *lv = writelistvalue(c->selectionlist, c->selectionindex, tbuf);
          c->texti = termi + lv->length;
          lv->code[4] = '-';
          lv->code[5] = INCALPHANUM(lv->priority);
          lv->code[6] = NUL;
          QUICKCMD(lv->source);
          lv->code[4] = NUL;
          freeselectionlist(c->selectionlist);
          c->selectionlist = NULL;
        } EL { // lost track of [ or nothing selected
          c->text[c->texti] = ch; // ' '
          c->text[INC c->texti] = NUL;
          freeselectionlist(c->selectionlist);
          c->selectionlist = NULL;
        }
      // ============ OPEN SELECTION BOX ON ] =========================
      } EF (ch EQ ']') { // ] will begin a selection
        IF (c->selectionlist) {
          eRs("selection while selecting!");
          freeselectionlist(c->selectionlist); // will replace with new one .. whatever 
        }
        IN termi = c->texti;
        WI (termi GQ 0 AND c->text[termi] NQ '[')
          { DEC termi; }
        IF (termi GQ 0 AND c->text[termi] EQ '[') {
          CS term = &c->text[termi + 1];
          c->selectionlist = pinyinselectionlist(term);
          c->selnumitems = getlistsize(c->selectionlist);
          c->selectionindex = 1;
        } // EL failed to select, probably "bla]" ... watch out for "[bla] bla]"
        WRITEBUFCH(c->text, c->texti, c->textsize, ch);
      // =============== WRITE CHAR TO CURRENT CONTEXT BUFFER ==================
      } EL { WRITEBUFCH(c->text, c->texti, c->textsize, ch); }
      // consider extending buffer instead of overflow - copy, double size
    }
  }
  stoppointer(&shell.pointer);
  destroyglyphfacelib(); 
//  destroyglyphlib(ftlib);
//  IF (mouse) { stopmouse(mouse); }
//  restoreterm();
  RT 0;
}
