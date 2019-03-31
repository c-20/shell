#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#define MAXGLYPHS	65535

IN setmin(IN value, INP minvalue) {
  IF (value LT *minvalue)
    { *minvalue = value; }
  RT value;
}
IN setmax(IN value, INP maxvalue) {
  IF (value GT *maxvalue)
    { *maxvalue = value; }
  RT value;
}
IN setminmax(IN value, INP minvalue, INP maxvalue) {
  RT setmin(setmax(value, maxvalue), minvalue);
}
FP setminf(FP value, FPP minvalue) {
  IF (value LT *minvalue)
    { *minvalue = value; }
  RT value;
}
FP setmaxf(FP value, FPP maxvalue) {
  IF (value GT *maxvalue)
    { *maxvalue = value; }
  RT value;
}
FP setminmaxf(FP value, FPP minvalue, FPP maxvalue) {
  RT setminf(setmaxf(value, maxvalue), minvalue);
}

FP getftpos(FT_Pos x) {
  RT ((FP)x) / 64.0; // / 4096.0f;
}

IN readglyph(CCS srcfile, IN unicode, FPP fpv, UCHP fiv, IN fic, FPP fcw, FPP fch);

IN readglyphsize(CCS srcfile, IN unicode, INP fpvsize, INP fivsize, FPP fcw, FPP fch) {
  FPP fpv = (FPP)(VDP)fpvsize;    // masquerade INP as FPP
  UCHP fiv = (UCHP)(VDP)fivsize;  // masquerade INP as UCHP
  // readglyph knows to treat fpv and fiv as INP to return size when fic is -1
  RT readglyph(srcfile, unicode, fpv, fiv, -1, fcw, fch); // fic should == *fivsize
}

FT_Library ftlib;
FT_Face ftface;
CH initglyphlibface(CS srcfile) {
  IF0EQ(FT_Init_FreeType(&ftlib)) {
    IN faceindex = 0; // re-using one face for all glyphs
    IF0NQ(FT_New_Face(ftlib, srcfile, faceindex, &ftface))
      { RT1("INITFONTFACEFAIL"); }
    FT_Set_Char_Size(ftface, 1, 1, 64, 64);
    RT 0;
  } EL { RT1("INITGLYPHLIBFAIL"); }
}

CH destroyglyphfacelib() {
  IF0NQ(FT_Done_Face(ftface))
    { RT1("FREEFONTFACEFAIL"); }
  IF0NQ(FT_Done_FreeType(ftlib))
    { RT1("FREEGLYPHLIBFAIL"); }
  RT 0;
}

// later: read glyph using particular FT_Face * (one per font)
IN readglyph(CCS srcfile, IN unicode, FPP fpv, UCHP fiv, IN fic, FPP fcw, FPP fch) {
// fpv/fiv are fpvsize/fivsize pointers if fic == -1 <----------------
  IN rtval = 0;
//  FT_Face ftface;
//  IF (FT_Init_FreeType(&ftlib))
//    { RT1("INITFREETYPEFAIL"); }
  // ----- reusing one global ftlib and ftface ------- also means one srcfile .....------
//  IN faceindex = 0; // 0 unless many faces
//  IF (FT_New_Face(ftlib, srcfile, faceindex, &ftface))
//    { Yi(faceindex); RT1("NEWFACEFAIL"); }
//  FT_Set_Char_Size(ftface, 1, 1, 64, 64);
  IN fpvi = -1;
  IN fivi = -1;
  IN glyphix = FT_Get_Char_Index(ftface, unicode);
  IF0EQ(FT_Load_Glyph(ftface, glyphix, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING | FT_LOAD_IGNORE_TRANSFORM)) {
    FP fchw = getftpos(ftface->glyph->advance.x);
    FP fchh = getftpos(ftface->glyph->advance.y); // returns 0
    FT_Outline *outline = &(ftface->glyph->outline);
    FP boxminx = 1000000.0f;
    FP boxminy = boxminx;
    FP boxmaxx = -boxminx;
    FP boxmaxy = boxmaxx;
    IN s = 0, con = -1;
    WI (INC con LT outline->n_contours) {
      IN pnts = 1;
      IN e = outline->contours[con] + 1;
      FP lastx = getftpos(outline->points[s].x);
      FP lasty = getftpos(outline->points[s].y);
      setminmaxf(lastx, &boxminx, &boxmaxx);
      setminmaxf(lasty, &boxminy, &boxmaxy);
      IF (fic NQ -1) {
        fiv[INC fivi] = 2;
        fpv[INC fpvi] = lastx;
        fpv[INC fpvi] = lasty;
      } EL { INC fivi; fpvi ADDS 2; } // ++ 1in, 2pt
      IN i = s + 1;
      WI (i LQ e) {
        IN c = (i EQ e) ? s : i;
        IN n = (i EQ e - 1) ? s : (i + 1);
        FP thisx = getftpos(outline->points[c].x);
        FP thisy = getftpos(outline->points[c].y);
        IF (outline->tags[c] & 1) { // line
          INC i;
          setminmaxf(thisx, &boxminx, &boxmaxx);
          setminmaxf(thisy, &boxminy, &boxmaxy);
          IF (fic NQ -1) {
            fiv[INC fivi] = 4;
            fpv[INC fpvi] = thisx;
            fpv[INC fpvi] = thisy;
          } EL { INC fivi; fpvi ADDS 2; } // ++ 1in, 2pt
          pnts ADDS 1; // ++ 1pair
        } EL {                    // spline
          FP nextx = 0.0f, nexty = 0.0f;
          IF (outline->tags[n] & 1) {
            nextx = getftpos(outline->points[n].x);
            nexty = getftpos(outline->points[n].y);
            i ADDS 2;
          } EL { // next off, use midpoint
            nextx = (thisx + getftpos(outline->points[n].x)) * 0.5f;
            nexty = (thisy + getftpos(outline->points[n].y)) * 0.5f;
            INC i;
          }
          setminmaxf(thisx, &boxminx, &boxmaxx);
          setminmaxf(thisy, &boxminy, &boxmaxy);
          setminmaxf(nextx, &boxminx, &boxmaxx);
          setminmaxf(nexty, &boxminy, &boxmaxy);
          IF (fic NQ -1) {
            fiv[INC fivi] = 10;
            fpv[INC fpvi] = thisx;
            fpv[INC fpvi] = thisy;
            fpv[INC fpvi] = nextx;
            fpv[INC fpvi] = nexty;
          } EL { INC fivi; fpvi ADDS 4; } // ++ 1in, 4pt
          pnts ADDS 2; // ++ 2pairs
        }
      }
      IF (fic NQ -1) {
        fiv[INC fivi] = 0;
      } EL { INC fivi; }
      s = e;
    }
    IF (fcw) { *fcw = fchw; } // boxmaxx - boxminx; }
    IF (fch) { *fch = boxmaxy - boxminy; } // since fchh is 0
    IF (fic EQ -1) {
      // set sizes
      INP fpvsize = (INP)(VDP)fpv;
      INP fivsize = (INP)(VDP)fiv;
      *fpvsize = fpvi + 1;
      *fivsize = fivi + 1;
      rtval = fivi + 1;
    } EL {
      IF (fic EQ fivi + 1) {
//        Gxc(96);
        rtval = fic;
      } EL {
        Rs("unexpected instruction count"); Rxc(97);
        rtval = -1; // unexpected instruction count - implies a memory problem
      }
    }
  } EL {
   Rs("ftloadglyph failed");
//    Dxc(96);
    rtval = 0; // 0 instructions
  }
//  FT_Done_Face(ftface); maybe keep it
//  FT_Done_FreeType(ftlib);
//M1("rtval %d", rtval);
  RT rtval;
}
