#include "utf8.h"
#include "readglyph.h"

// actually 0-35, not just 0-15
IN hexthischar(CH ch) {
  IF INRANGE(ch, A0, A9) {
    RT ch - A0;
  } EF INRANGE(ch, AA, AZ) {
    RT ch - AA + 10;
  } EF INRANGE(ch, Aa, Az) {
    RT ch - Aa + 10;
  } EL { RT -1; }
}

typedef struct _List {
  IN index;
  CS source;
  CS code;
  CS item;
  IN value;
  IN length;
  CH priority;
  IN toneindex;
  struct _List *prev;
  struct _List *next;
} List;
List *pinyinselectionlist(CS term) {
  List *list = NULL;
#define PCALLMAXSIZE	50
  CH pcall[PCALLMAXSIZE];
  snprintf(pcall, PCALLMAXSIZE, "pinyin %s", term);
  FS pcmd = OPENCMD(pcall);
  IF (!pcmd) { Rs("pinyinfail"); _exit(10); }
  IN pvalue = 0;
#define PNAMEMAXSIZE	30
  CH pname[PNAMEMAXSIZE];
  CH thiscode[4]; // 4 bytes for 16-bit UTF index
  IN pnamei = 0;
  IN priority = 0;
  IN toneix = 0;
  IN optix = 0;
  IN line = 0;
  IN column = 0;
  IN numopts = 0;
  IN pch = NUL;
  WI (pch NQEOF) { // could be set by inner loop
    pch = GETFCH(pcmd);
    BKEQEOF(pch);
    IF (INC column LQ 6) {
      IN phex = hexthischar(pch);
      IF (INRANGE(column, 1, 5) AND phex EQ -1)
        { BK; } // assume end of list if no NFFFF prefix
      EF (column EQ 1) { priority = pch;       }
      EF (column EQ 2) { thiscode[0] = pch; pvalue += phex << 12; }
      EF (column EQ 3) { thiscode[1] = pch; pvalue += phex << 8;  }
      EF (column EQ 4) { thiscode[2] = pch; pvalue += phex << 4;  }
      EF (column EQ 5) { thiscode[3] = pch; pvalue += phex;       }
      EF (column EQ 6 AND phex EQ -1) {
        INC line;
        WI (1) {
          pch = GETFCH(pcmd);
          BKEQEOF(pch);
          IF (pch EQ Anewline OR pch EQ Aleftbracket) {
            IF (pch EQ Aleftbracket) {
              pch = GETFCH(pcmd);
              IF INRANGE(pch, A0, A9) { // allow tones 1 - 9 (tone 0 = out of map)
                toneix = pch - A0;
                pch = GETFCH(pcmd);
                IF (pch EQ Arightbracket) {
                  pch = GETFCH(pcmd);
                  IF (pch NQ Anewline)
                    { pch = EOF; BK; } // unexpected extra char
                } EL { pch = EOF; BK; } // unclosed bracket
              } EL { pch = EOF; BK; } // invalid tone index, end list
            } ; IF (pch NQ Anewline) { pch = EOF; BK; }
            IF STREQ(term, pname) {
              List *l = (List *)calloc(1, sizeof(List));
              l->index = INC optix; // from 1 to numopts
              l->source = (CS)calloc(7 + 4 + 2 + 1, sizeof(CH)); // pinyin FFFF-0
              sprintf(l->source, "pinyin ");
              l->code = &(l->source[9]);
              sprintf(l->code, "%c%c%c%c", thiscode[0], thiscode[1], thiscode[2], thiscode[3]);
              l->item = (CS)calloc(5, sizeof(CH)); // 1 - 4 bytes
              l->value = pvalue;
              l->length = utf8thischar(pvalue, l->item);
              l->priority = priority;
              l->toneindex = toneix;
              l->prev = list;
              IF (l->prev) 
                { l->prev->next = l; }
              l->next = NULL;
              WI (l->prev AND l->priority GT l->prev->priority) {
                List *lp = l->prev;
                lp->next = l->next;
                l->prev = lp->prev;
                lp->prev = l;
                l->next = lp;
                IN swapindex = lp->index;
                lp->index = l->index;
                l->index = swapindex;
                IF (lp->next)
                  { lp->next->prev = lp; }
                IF (l->prev)
                  { l->prev->next = l; }
              } // sort by priority
              list = l; // point to the added item
              WI (list->next) // fwd to end of list
                { list = list->next; }
            } // EL pinyin not matching, next
            column = 0;
            pvalue = 0;
            priority = 0;
            toneix = 0;
            CLEARBUF(pname, pnamei);
            BK;
          } EL { WRITEBUFCH(pname, pnamei, PNAMEMAXSIZE, pch); }
        } ; IF (pch NQ Anewline) { Rxc(164); Ri(pch); }
      } EL { Rxc(164); Mc(pch); BK; } // invalid header, not "FFFFF "
    } EL {
      Rxc(164); Rc(pch); // unhandled value, this shouldn't happen
    }
  }
  CLOSECMD(pcmd);
  WI (list AND list->prev) // rewind list
    { list = list->prev; }
  RT list;
//  RT returnlist; // ... <- (1) -> N
}
VD freeselectionlist(List *list) {
  // de-allocate full list from end to start
  List *l = list;
  WI (l->next) { l = l->next; }
  WI (l) {
    List *nl = l->prev;
    free(l->source);
    free(l->item);
    free(l);
    l = nl;
  }
}
List *writelistvalue(List *list, IN index, CS tobuf) {
  IF (!list) { RT NULL; }
  WI (list->prev)
    { list = list->prev; }
  WI (list) {
    IF (list->index EQ index) {
      IF (tobuf)
        { sprintf(tobuf, "%s", list->item); }
      RT list;
    }
    list = list->next;
  }
  RT 0;
}
List *getlistitem(List *list, IN index) {
  RT writelistvalue(list, index, NULL);
}
IN getlistsize(List *list) {
  IF (!list) { RT 0; }
  WI (list->prev)
    { list = list->prev; }
  IN listsize = 1;
  WI (list->next)
    { INC listsize; list = list->next; }
  RT listsize;
}

typedef struct _glyph {
  CCS srcfile;
  IN charindex;
  FPP pointvalues;
  UCHP instvalues;
  IN instcount;
  FP charwidth;
  FP charheight;
  IN countindex;
  struct _glyph *next;
} glyph;
glyph *glyphs = NULL;
IN numglyphs = 0;

IN reloadglyph(CCS srcfile, IN index, FPP *fpvp, UCHP *fivp, IN *ficp, FP *fcwp, FP *fchp) {
  glyph *g = glyphs;
  WI (g NQNULL) {
    IF (g->srcfile EQ srcfile AND g->charindex EQ index) {
      IF (fpvp) { *fpvp = g->pointvalues; }
      IF (fivp) { *fivp = g->instvalues; }
      IF (ficp) { *ficp = g->instcount; }
      IF (fcwp) { *fcwp = g->charwidth; }
      IF (fchp) { *fchp = g->charheight; }
      RT g->countindex; // should be > 0
//      RT g->instcount; // retrieved from cache
    } EL { g = g->next; }
  }; RT 0; // not found
}

IN saveglyph(CCS srcfile, IN index, FPP fpv, UCHP fiv, IN fic, FP fcw, FP fch) {
  IN cnumber = reloadglyph(srcfile, index, NULL, NULL, NULL, NULL, NULL);
  IF (cnumber EQ 0) {
    glyph *g = (glyph *)malloc(sizeof(glyph));
    IF (g EQNULL) { Rxc(96); RT -1; } // out of memory
    g->srcfile = srcfile;
    g->charindex = index;
    g->pointvalues = fpv;
    g->instvalues = fiv;
    g->instcount = fic;
    g->charwidth = fcw;
    g->charheight = fch;
    g->countindex = INC numglyphs; // starts at 1
    g->next = glyphs;
    glyphs = g; // newest first
//    Gxc(96); // added to cache
//Mi(g->countindex);
    RT g->countindex;
  } EL {
    Yxc(96); // already in cache
//Ci(cnumber);
    RT cnumber;
  }
}

// consider loadglyphsize(CCS srcfile, IN index, FPP *fpvp, UCH

IN loadglyph(CCS srcfile, IN index, FPP *fpvp, UCHP *fivp, IN *ficp, FP *fcwp, FP *fchp) {
  IN fpvsize = 0, fivsize = 0;
  FP fcwidth = 0.0f, fcheight = 0.0f;
  IN cnumber = reloadglyph(srcfile, index, fpvp, fivp, ficp, fcwp, fchp);
  IF (cnumber GT 0) { // found, was loaded
//Cxc(96);
    RT cnumber; // fivp, ficp, fcwp, fchp should be setY1("ic: %d", *ficp);
  } EL { // not found, load fresh
    IN fic = readglyphsize(srcfile, index, &fpvsize, &fivsize, &fcwidth, &fcheight);
// no, this will return random sizes ...................................... actually no
// make sure it returns fic tho
    IF (fic NQ fivsize)
      { RLOG2("!EQ fic=%d fivsize=%d", fic, fivsize); }
    IF (fic GT 0) {
      *fpvp = malloc(fpvsize * sizeof(FP));
      *fivp = malloc(fivsize * sizeof(UCH));
    } // only deallocate if > 0 was allocated (can't allocate 0)
    IF (fcwp) { *fcwp = fcwidth; }
    IF (fchp) { *fchp = fcheight; }
    IN fread = readglyph(srcfile, index, *fpvp, *fivp, fic, NULL, NULL);
    IF (fic EQ fread) {
      *ficp = fic;
      IF (0 GT saveglyph(srcfile, index, *fpvp, *fivp, fic, fcwidth, fcheight))
        { Mc('!'); } // glyph failed to save
      RT cnumber;
//      RT fic;
    } EL { R2("!EQ rgsfic=%d fread=%d GLYPHNOTLOADED", fic, fread); RT 0; } // RT fread; }
  }
}

VD freeglyph(FPP fpv, UCHP fiv)
  { free(fpv); free(fiv); }

// remember: srcfile is font src file
FP drawsomechars(CCS srcfile, IN ptsize, FP x, FP y, CCS textstr, CH visible) {
  FP size = (FP)ptsize;
  FP xx = x;
  FP modelview[9];
  vgGetMatrix(modelview);
  FP fch = 0.0f;
  IN character;
  UCS ss = (UCS)textstr;
  IF (ss EQNULL) { ss = "[]"; } // full-height chars for height calculation
  WI ((ss = nextutf8char(ss, &character)) NQNULL) {
    FPP fpv = NULL;
    UCHP fiv = NULL;
    IN fic = 0;
    FP fcw = 0.0f;
    IF (0 LQ loadglyph(srcfile, character, &fpv, &fiv, &fic, &fcw, &fch)) {
      IF (fic GT 0 AND textstr NQNULL AND visible EQ 1) { // do not draw [] or invisible
        VGPath path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
                                   1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);
        vgAppendPathData(path, fic, fiv, fpv);
//        freeglyph(fpv, fiv); <- assume cached, alt: decrement accessing resource count..
        FP offset[9] =
          { size, 0.0f, 0.0f,
            0.0f, size, 0.0f,
            xx,   y,    1.0f  };
        vgLoadMatrix(modelview);
        vgMultMatrix(offset);
        vgDrawPath(path, VG_FILL_PATH);
        vgDestroyPath(path);
      }
      xx += size * fcw; // 0 width char should work because it uses advance.x
    } EL {
      RLOG("LOADGLYPHFAIL");
    }
  }
  vgLoadMatrix(modelview);
  IF (textstr EQNULL)
    { RT fch * size; } // height if textstr EQNULL (height calculation)
  EL { RT xx - x; }    // width if textstr NQNULL (text written)
}

FP drawchars(CCS srcfile, IN ptsize, FP x, FP y, CCS textstr) {
  drawsomechars(srcfile, ptsize, x, y, textstr, 1);
}

FP drawnochars(CCS srcfile, IN ptsize, FP x, FP y, CCS textstr) {
  drawsomechars(srcfile, ptsize, x, y, textstr, 0);
}

FP drawlineswh(CCS srcfile, IN ptsize, FP x, FP y, CS str, FPP w, FPP h) {
  // if h is a pointer, do not draw - just calculate size. height also returned
  // still draws when w is a pointer, saves w,h if pointers, returns height
  FP lineheight = drawchars(srcfile, ptsize, 0, 0, NULL);
  FP maxwidth = 0.0f;
  FP height = 0.0f;
  WI (str[0] NQNUL) {
    CS endp = str;
    WI (*endp NQNUL AND *endp NQ '\r') // \r replaces \n .. '\n')
      { INC endp; }
    FP thiswidth = 0.0f;
    IF (*endp EQ '\r') { // \r replaces \n ... '\n') { // line
      *endp = NUL; // <--------------- dynamic strings only!
      height ADDS lineheight;
      IF (h) { thiswidth = drawnochars(srcfile, ptsize, x, y - height, str); }
      EL { thiswidth = drawchars(srcfile, ptsize, x, y - height, str); }
      *endp = '\r'; // restore
      str = endp + 1; // after NUL
    } EL { // last line (no \n expected)
      height ADDS lineheight;
      IF (h) { thiswidth = drawnochars(srcfile, ptsize, x, y - height, str); }
      EL { thiswidth = drawchars(srcfile, ptsize, x, y - height, str); }
      str = endp; // at NUL
    }
    IF (thiswidth GT maxwidth) { maxwidth = thiswidth; }
  }
  IF (w) { *w = maxwidth; }
  IF (h) { *h = height; }
  RT height;
}

FP drawlinesw(CCS srcfile, IN ptsize, FP x, FP y, CS str, FPP w) {
  RT drawlineswh(srcfile, ptsize, x, y, str, w, NULL);
}

FP drawlines(CCS srcfile, IN ptsize, FP x, FP y, CS str) {
  RT drawlineswh(srcfile, ptsize, x, y, str, NULL, NULL); // returns height
}

FP drawfile(CCS srcfile, IN ptsize, FP x, FP y, FS file) {
  FP xx = x, yy = y;
//  FP height = 0.0f;
  FP lineheight = drawchars(srcfile, ptsize, 0, 0, NULL);
  WI (file) {
    CH code[5]; // 1-4 chars
    IN ch = GETFCH(file);
    BKEQEOF(ch);
    IF ((ch & 0xE0) EQ 0xC0) { // 110xxxxx 10xxxxxx
      IN ch2 = GETFCH(file);
      IFEQEOF(ch2) { Yi(2); Ri(2); BK; } // unexpected EOF at char 2 of 2
      code[0] = ch; code[1] = ch2; code[2] = NUL;
    } EF ((ch & 0xF0) EQ 0xE0) { // 1110xxxx 10xxxxxx 10xxxxxx
      IN ch2 = GETFCH(file);
      IFEQEOF(ch2) { Yi(3); Ri(2); BK; } // unexpected EOF at char 2 of 3
      IN ch3 = GETFCH(file);
      IFEQEOF(ch3) { Yi(3); Ri(3); BK; } // unexpected EOF at char 3 of 3
      code[0] = ch; code[1] = ch2; code[2] = ch3; code[3] = NUL;
    } EF ((ch & 0xF8) EQ 0xF0) { // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
      IN ch2 = GETFCH(file);
      IFEQEOF(ch2) { Yi(4); Ri(2); BK; } // unexpected EOF at char 2 of 4
      IN ch3 = GETFCH(file);
      IFEQEOF(ch3) { Yi(4); Ri(3); BK; } // unexpected EOF at char 3 of 4
      IN ch4 = GETFCH(file);
      IFEQEOF(ch4) { Yi(4); Ri(4); BK; } // unexpected EOF at char 4 of 4
      code[0] = ch; code[1] = ch2; code[2] = ch3; code[3] = ch4; code[4] = NUL;
    } EL { code[0] = ch; code[1] = NUL; } // 0xxxxxxx
    IF (ch NQ '\n') {
      xx += drawchars(srcfile, ptsize, xx, yy, code);
    } EL { xx = x; yy -= lineheight; }
  }
  GOTOSOF(file); // reset seek pointer
  RT yy - y; // height
}

FP drawlist(CCS srcfile, IN ptsize, FP x, FP y, List *list, IN cols, FP minwidth, FP minheight) {
  FP xx = x, yy = y; // negative minwidth for square characters
//  FP height = 0.0f;
  FP lineheight = drawchars(srcfile, ptsize, 0, 0, NULL);
  IN colix = 0, rowix = 1;
  WI (list) {
    IF (INC colix GT cols) {
      colix = 1; INC rowix;
      xx = x; yy -= (minheight GT lineheight) ? minheight : lineheight;
    }
    FP offset = drawchars(srcfile, ptsize, xx, yy, list->item);
    xx += (minwidth LT 0.0f) ? lineheight : (offset LT minwidth) ? minwidth : offset;
    list = list->next;
  }
  RT yy - y; // height
}

FP drawcmd(CCS srcfile, IN ptsize, FP x, FP y, CS cmdstr, FP minwidth, FP minheight) {
  FP xx = x, yy = y; // negative minwidth for lineheight width
//  FP height = 0.0f;
  FP lineheight = drawchars(srcfile, ptsize, 0, 0, NULL);
  FS file = OPENCMD(cmdstr);
  IF (!file) { RT 0.0f; }
  IN colix = 0, rowix = 1;
  WI (file) {
    CH code[5]; // 1-4 chars
    IN ch = GETFCH(file);
    BKEQEOF(ch);
    IF ((ch & 0xE0) EQ 0xC0) { // 110xxxxx 10xxxxxx
      IN ch2 = GETFCH(file);
      IFEQEOF(ch2) { Yi(2); Ri(2); BK; } // unexpected EOF at char 2 of 2
      code[0] = ch; code[1] = ch2; code[2] = NUL;
    } EF ((ch & 0xF0) EQ 0xE0) { // 1110xxxx 10xxxxxx 10xxxxxx
      IN ch2 = GETFCH(file);
      IFEQEOF(ch2) { Yi(3); Ri(2); BK; } // unexpected EOF at char 2 of 3
      IN ch3 = GETFCH(file);
      IFEQEOF(ch3) { Yi(3); Ri(3); BK; } // unexpected EOF at char 3 of 3
      code[0] = ch; code[1] = ch2; code[2] = ch3; code[3] = NUL;
    } EF ((ch & 0xF8) EQ 0xF0) { // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
      IN ch2 = GETFCH(file);
      IFEQEOF(ch2) { Yi(4); Ri(2); BK; } // unexpected EOF at char 2 of 4
      IN ch3 = GETFCH(file);
      IFEQEOF(ch3) { Yi(4); Ri(3); BK; } // unexpected EOF at char 3 of 4
      IN ch4 = GETFCH(file);
      IFEQEOF(ch4) { Yi(4); Ri(4); BK; } // unexpected EOF at char 4 of 4
      code[0] = ch; code[1] = ch2; code[2] = ch3; code[3] = ch4; code[4] = NUL;
    } EL { code[0] = ch; code[1] = NUL; } // 0xxxxxxx
    IF (ch NQ '\n') {
      INC colix;
      FP offset = drawchars(srcfile, ptsize, xx, yy, code);
      xx += (minwidth LT 0.0f) ? lineheight : (offset LT minwidth) ? minwidth : offset;
    } EL {
      colix = 0; INC rowix;
      xx = x; yy -= (minheight GT lineheight) ? minheight : lineheight;
    }
  }
  RT yy - y; // height
}
