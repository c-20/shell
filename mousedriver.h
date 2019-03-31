#define LEFTCLICK   1
#define MIDDLECLICK 4
#define RIGHTCLICK  2

FS startmouse() {
//  FS mouse = OPENCMD("mousedriver");
  FS mouse = OPENCMD("mousdrv < /dev/input/mice");
  IN mfd = fileno(mouse);
  IN flags = fcntl(mfd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(mfd, F_SETFL, flags);
  RT mouse;
}

VD stopmouse(FS stream) {
  IF (stream) { CLOSECMD(stream); }
}

VD updatemouse(FS stream, IN *x, IN *y, IN *click) {
  // returns if a click was detected and updates the x, y position pointers
  IF (feof(stream)) { RLOG("FEOF"); RT; } // not expected
  IN clickcount = 0;
  IN ch = NUL;
  WI ( !( (ch = GETFCH(stream)) EQEOF OR (ch EQNUL AND errno EQ EWOULDBLOCK) ) ) {
    IF (ch EQ '[') {
      CH closed = An;
      INP target = NULL;
      IN value = 0;
      CH negative = 0;
      WI (ch NQ '\n') { // (1) {
        ch = GETFCH(stream);
        IF (ch EQ '\n') { BK; }
        IF (ch EQEOF) { Rxc(96); Rc(Am); BK; } // unexpected EOF
        EF (ch EQ ']') { closed = Ay; }
        IF (ch EQ ',' OR ch EQ ']') {
          IF (target NQNULL)
            { *target = value; }
          target = NULL;
        } EL {
          IF (target EQNULL AND ch EQ 'x') { target = x;     } 
          EF (target EQNULL AND ch EQ 'y') { target = y;     } 
          EF (target EQNULL AND ch EQ 'c') { target = click; }
          EF (target EQNULL AND ch EQ 'n') {
            target = &clickcount; // internal
          } EF (target NQNULL AND ch EQ '=') {
            value = 0;
          } EF (target NQNULL AND ch EQ '-') {
            negative = 1; // val=- will be the same as val=0
          } EF (target NQNULL AND INRANGE(ch, A0, A9)) {
            value = (value * 10) + (ch - A0);
            IF (value GT 0 AND negative EQ 1)
              { value MULS -1; negative = 0; }
          } EL { Rc('!'); Yi(ch); Rc(Am); } // unexpected char
        }
      }
    } EL { Rc('['); Rc(Am); } // expected [
  }
}

typedef struct _Pointer {
  FS mousefile;
  XY mousepos;
  XY mouseoffset;
  XYWH mousebox;
  XY mousecurve;
  IN mouseclick;
} Pointer;

VD initpointer(Pointer *p) {
  p->mousepos.x = 0;
  p->mousepos.y = 0;
  p->mouseoffset.x = -8;
  p->mouseoffset.y = -8;
  p->mousebox.x = 0 + p->mouseoffset.x;
  p->mousebox.y = 0 + p->mouseoffset.y;
  p->mousebox.w = 16;
  p->mousebox.h = 16;
  p->mousecurve.x = 16;
  p->mousecurve.y = 16;
  p->mouseclick = 0;
  p->mousefile = startmouse();
  IF (!p->mousefile) { RLOG("NOMOUSE"); }
}

VD updatepointer(Pointer *p) {
  updatemouse(p->mousefile, &p->mousepos.x, &p->mousepos.y, &p->mouseclick);
  p->mousebox.x = p->mousepos.x + p->mouseoffset.x;
  p->mousebox.y = p->mousepos.y + p->mouseoffset.y;
}

VD stoppointer(Pointer *p) {
  IF (p->mousefile)
    { stopmouse(p->mousefile); }
}
/*
IN updatemouse(FS stream, IN *x, IN *y, IN *click) {
  // returns if a click was detected and updates the x, y position pointers
  IF (feof(stream)) { RLOG("FEOF"); RT 0; } // not expected
  IN ch = GETFCH(stream);
  IF (ch EQNUL AND errno EQ EWOULDBLOCK) {
    RT 0; // empty, no data waiting
  } EF (ch EQEOF) {
    RT 0; // empty, no new data
  } EF (ch EQ '[') {
    CH closed = An;
    INP target = NULL;
    IN value = 0;
    WI (ch NQ '\n') {
      ch = GETFCH(stream);
      IF (ch EQ '\n') { BK; }
      IF (ch EQEOF) {
YLOG("NOCHAR");
  BK;
      } EF (ch EQ ']')
        { closed = Ay; }
      IF (ch EQ ',' OR ch EQ ']') {
        IF (target NQNULL)
          { *target = value; }
        target = NULL;
      } EL {
        IF (target EQNULL AND ch EQ 'x') {
          target = x;
        } EF (target EQNULL AND ch EQ 'y') {
          target = y;
        } EF (target EQNULL AND ch EQ 'c') {
          target = click;
        } EF (target NQNULL AND ch EQ '=') {
          value = 0;
        } EF (target NQNULL AND INRANGE(ch, A0, A9)) {
          value = (value * 10) + (ch - A0);
        } EL {
//          Rc('!'); Mc(ch); // unexpected character
        }
      }
    }
  }
  RT 1;
}
*/

