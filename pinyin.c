#include "code.h"
#define pinyin main
IN pinyin($) { // FFFF- for pinyin(t)-w
  CS pinyinfile = "/font/pinyin.utf";
  CH code[6];
  code[5] = NUL;
  CS unicode = &code[1];
  CH codepriority = An;
  WI ($N GT 0) {
    CS word = NULL;
    CH tone = 0;
    FS p = MODIFYFILE(pinyinfile);
    IF (!p) { RT 1; } // cannot open file for read/write <-------
    CH stat = An; // new line
    IN linenum = 1;
    WI NOTFEOF(p) {
      IN ch = GETFCH(p);
      IF (ch EQEOF) { BK; } // stop here //Rs("chEOF"); BK; } // undetected EOF!
      IF (ch EQ '\r' OR ch EQ '\n') {
//IF (stat EQ Am) {
//  GLOG("*\n");
//} EL { LOG("\n"); }
        IF (stat EQ Ac AND codepriority EQ Ay)
          { LOG1("-%c", code[0]); }
        codepriority = An;
        stat = An; // new line
        INC linenum;
      } EL {

//IF (stat NQ Am) {
//  G1("%c", stat);
//  IF (!ISLETTER(ch)) {
//    C1("[%d]", ch);
//  }
//  Y1("%c ", ch);
//}
//Yc(ch);
        IF (stat EQ An OR stat EQ Ap) { // newline or startoflinepadding
          IF (ch EQ '#') { stat = Am; } // mute
          EF (ch EQ ' ') { stat = Ap; } // padding
          EF (ch EQ '\t') { stat = Ap; } // padding
          EF ISALPHANUM(ch) { code[0] = ch; stat = A0; }
          EL { R2("A0 %d l%d", ch, linenum); stat = Ae; } // error: expected code char
        } EF (stat EQ A0) {
          IF ISHEXCHAR(ch) { code[1] = ch; stat = A1; }
          EL { stat = Ae; } // error: expected code char
        } EF (stat EQ A1) {
          IF ISHEXCHAR(ch) { code[2] = ch; stat = A2; }
          EL { stat = Ae; } // error: expected code char
        } EF (stat EQ A2) {
          IF ISHEXCHAR(ch) { code[3] = ch; stat = A3; }
          EL { stat = Ae; } // error: expected code char
        } EF (stat EQ A3) {
          IF ISHEXCHAR(ch) { code[4] = ch; stat = A4; }
          EL { stat = Ae; } // error: expected code char
        } EF (stat EQ A4) {
          IF (INRANGE4($1[0], $1[1], $1[2], $1[3], A0, AZ) AND ($1[4] EQNUL OR $1[4] EQ Adash)) {
            // if matchword is an uppercase code, check it and move to next param
            IF SEQ4(unicode, $1[0], $1[1], $1[2], $1[3]) {
              stat = Ac; // matching code
              IF ($1[4] EQ Adash) { codepriority = Ay; }
              IF (codepriority EQ Ay AND $1[5] NQNUL) // update priority [P1234 ]<-
                { STEPF(p, -6); FCHOUT(p, $1[5]); STEPF(p, 5); code[0] = $1[5]; }
            } EL { stat = Am; } // muted code
          } EF ISPADCHAR(ch) { stat = Ad; } // delimeter
          EL { stat = Ae; } // error: expected delimieter
        } EF (stat EQ Aw) {
          IF ISPADCHAR(ch) {
            stat = Ad; // next match, continue
          }
        } EF (stat EQ Ar) {
//R1("%c", ch);
          IF ISLETTER(ch) {
            // next match character
//IF (word) { C1("%s", word); } EL { Rs("noword"); }
            IF LOWEREQ(ch, word[0]) {
              stat = Ar; // no change, continue
              INC word;
            } EL {
              stat = Aw; // not a match, wait for next
              word = NULL;
            }
          } EF ISNUMBER(ch) {
            // tone marker index, end of match
            tone = ch - '0';
            IF (word[0] EQNUL) {
              // this is a match
              LOGF("%c%c%c%c%c %s(%d)\n", code[0], code[1], code[2], code[3], code[4], $1, tone);
            }
          } EF ISPADCHAR(ch) {
            // end of match
            IF (word[0] EQNUL) {
              // this is a match
              LOGF("%c%c%c%c%c %s\n", code[0], code[1], code[2], code[3], code[4], $1);
            }
          } // EL { R1("wut %c", ch); } <-- matches ^ sometimes ...?
        } EF (stat EQ Ad OR stat EQ Ac) {
          IF ISPADCHAR(ch) {
            stat = stat; // no change, continue
          } EF (stat EQ Ac AND ISLETTER(ch)) {
            LOGF("%c", ch);
          } EF (stat EQ Ac AND INRANGE(ch, A0, A9)) {
            LOG1("(%c)", ch); // D G M B R for tones - 1 2 3 4
          } EF (stat EQ Ac) {
            Rxc(97); Yi(ch); Rc('p'); // unexpected character
          } EF ISLETTER(ch) {
            word = $1;
            IF LOWEREQ(ch, word[0]) {
              stat = Ar; // reading matchword
              INC word;
            } EL {
              word = NULL;
              stat = Aw; // waiting for next match
            }
          } EL {
            stat = Ae; // unexpected character    <- log, then
            Yxc(145); // UNEXPECTEDCHARACTER
            Y1(":%d", linenum);
            stat = Aw; // wait for next match tho <- continue
          }
        } EF (stat EQ Am) {
//MLOG("*");
        } EL {
          RLOG1("Unmanaged status %c.\n",  stat);
          stat = Ae; // unmanaged state
          RT 1;
        }
      }
    }
//    LOG("\n");
    CLOSEFILE(p); // hopefully write will occur
    $$1;
  }  
  RT 0;
}
