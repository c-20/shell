
VD lfaddmapfocus(Parameter *cpr, LF x, LF y) {
  LFList *M1xfocus  = lfgetparam(cpr, "M1.XFOCUS");
  LFList *M1yfocus  = lfgetparam(cpr, "M1.YFOCUS");
  LFList *M1xoffset = lfgetparam(cpr, "M1.XOFFSET");
  LFList *M1yoffset = lfgetparam(cpr, "M1.YOFFSET");
  LFList *M1xvalue  = lfgetparam(cpr, "M1.XVALUE");
  LFList *M1yvalue  = lfgetparam(cpr, "M1.YVALUE");
  LF focusx = x;
  LF focusy = y;
  LF offsetx = { 0, 0 };
  LF offsety = { 0, 0 };
  lfaddvalue(M1xfocus, focusx, "*");
  lfaddvalue(M1yfocus, focusy, "*");
  lfaddvalue(M1xoffset, offsetx, "*");
  lfaddvalue(M1yoffset, offsety, "*");
  LF valuex = LFADD(focusx, offsetx);
  LF valuey = LFADD(focusy, offsety);
  lfaddvalue(M1xvalue, valuex, "*");
  lfaddvalue(M1yvalue, valuey, "*");
}


VD lfaddmapoffset(Parameter *cpr, LF x, LF y) {
  LFList *M1xfocus  = lfgetparam(cpr, "M1.XFOCUS");
  LFList *M1yfocus  = lfgetparam(cpr, "M1.YFOCUS");
  LFList *M1xoffset = lfgetparam(cpr, "M1.XOFFSET");
  LFList *M1yoffset = lfgetparam(cpr, "M1.YOFFSET");
  LFList *M1xvalue  = lfgetparam(cpr, "M1.XVALUE");
  LFList *M1yvalue  = lfgetparam(cpr, "M1.YVALUE");
  LF focusx = lfgetlastlfvalue(M1xfocus);
  LF focusy = lfgetlastlfvalue(M1yfocus);
  LF offsetx = x;
  LF offsety = y;
//  lfaddvalue(M1xfocus, focusx, "+);  // no shifting focus
//  lfaddvalue(M1yfocus, focusy, "+");  // ..yet
  lfaddvalue(M1xoffset, offsetx, "+");
  lfaddvalue(M1yoffset, offsety, "+");
  LF valuex = LFADD(focusx, offsetx);
  LF valuey = LFADD(focusy, offsety);
  lfaddvalue(M1xvalue, valuex, "+");
  lfaddvalue(M1yvalue, valuey, "+");
}

VD lfupdatemapcoords(Parameter *spr, Parameter *cpr) {
  // shell screen get params
  LF S1pixelw  = lfgetlfparam(spr, "S1.WIDTH");
  LF S1pixelh  = lfgetlfparam(spr, "S1.HEIGHT");
  LF S1boundl  = lfgetlfparam(spr, "S1.BOUNDL");
  LF S1boundr  = lfgetlfparam(spr, "S1.BOUNDR");
  LF S1boundt  = lfgetlfparam(spr, "S1.BOUNDT");
  LF S1boundb  = lfgetlfparam(spr, "S1.BOUNDB");
  // context map get params
  LFList *M1xvalue  = lfgetparam(cpr, "M1.XVALUE");
  LFList *M1yvalue  = lfgetparam(cpr, "M1.YVALUE");
  // context update bounds
  LF M1boundl       = lfvaluesmin(M1xvalue);
  LF M1boundr       = lfvaluesmax(M1xvalue);
  LF M1boundt       = lfvaluesmin(M1yvalue);
  LF M1boundb       = lfvaluesmax(M1yvalue);
//Clf(M1boundl);
//Rlf(M1boundr);
//Clf(M1boundt);
//Ylf(M1boundb);
// set as M1.BOUNDLRTB as well .............................. ^
  // screen update bounds
  IF LFLT(M1boundl, S1boundl)
    { S1boundl = lfsetlfparamlf(spr, "S1.BOUNDL", M1boundl); }
  IF LFGT(M1boundr, S1boundr)
    { S1boundr = lfsetlfparamlf(spr, "S1.BOUNDR", M1boundr); }
  IF LFLT(M1boundt, S1boundt)
    { S1boundt = lfsetlfparamlf(spr, "S1.BOUNDT", M1boundt); }
  IF LFGT(M1boundb, S1boundb)
    { S1boundb = lfsetlfparamlf(spr, "S1.BOUNDB", M1boundb); }
// consider labelling the changes "+" and "-" (currently no "-")
  // screen calculate increment
  LF S1offsetw = LFSUB(S1boundr, S1boundl);
  LF S1offseth = LFSUB(S1boundb, S1boundt); // or reverse
  S1offsetw = lfsetlfparamlf(spr, "S1.OFFSETW", S1offsetw);
  S1offseth = lfsetlfparamlf(spr, "S1.OFFSETH", S1offseth);

//eY2("SO %lld,%lld", S1offsetw.numb, S1offseth.numb);
//Clf(S1offsetw);
//Ylf(S1offseth); .. will have to trust the values, no eYlf()
  LF S1incremx = LFDIV(S1offsetw, S1pixelw);
  lfsetlfparamlf(cpr, "S1.INCREMX", S1incremx);
  LF S1incremy = LFDIV(S1offseth, S1pixelh);
  lfsetlfparamlf(cpr, "S1.INCREMY", S1incremy);
//Ms("SIxy");
//Rlf(S1incremx);
//Ylf(S1incremy);
//eB2("SI %lld,%lld", S1incremx.numb, S1incremy.numb);

  // context update offsets
  LF M1offsetl = LFSUB(M1boundl, S1boundl);
  lfsetlfparamlf(cpr, "M1.OFFSETL", M1offsetl);
  LF M1offsetr = LFSUB(S1boundr, M1boundr);
  lfsetlfparamlf(cpr, "M1.OFFSETR", M1offsetr);
  LF M1offsetw = LFSUB(M1boundr, M1boundl);
  lfsetlfparamlf(cpr, "M1.OFFSETW", M1offsetw);
  LF M1offsett = LFSUB(M1boundt, S1boundt);
  lfsetlfparamlf(cpr, "M1.OFFSETT", M1offsett);
  LF M1offsetb = LFSUB(S1boundb, M1boundb);
  lfsetlfparamlf(cpr, "M1.OFFSETB", M1offsetb);
  LF M1offseth = LFSUB(M1boundb, M1boundt);
  lfsetlfparamlf(cpr, "M1.OFFSETH", M1offseth);
  // context update position
  LF M1pixell = LFMUL(M1offsetl, S1incremx);
  lfsetlfparamlf(cpr, "M1.PIXELL", M1pixell);
  LF M1pixelt = LFMUL(M1offsett, S1incremy);
  lfsetlfparamlf(cpr, "M1.PIXELT", M1pixelt);
  LF M1pixelw = LFMUL(M1offsetw, S1incremx);
  lfsetlfparamlf(cpr, "M1.PIXELW", M1pixelw);
  LF M1pixelh = LFMUL(M1offseth, S1incremy);
  lfsetlfparamlf(cpr, "M1.PIXELH", M1pixelh);
//Cs("+++");
//Wlf(M1pixell);
//Ylf(M1pixelt);
//Wlf(M1pixelw);
//Blf(M1pixelh);
//Cs("---");
}

