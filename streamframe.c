#include "code.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <sched.h>

#include "strequal.h"

// ---------------------------------------------------
// channel context --- me to you
// breaking [out in(to)] tables
// ----------------------------------------------------
#define STREAMSRCDIR	"/V:"
//#define STREAMSRCID	"0"
#define STREAMTGTID	"1"
// auto-increment IDs with multiple instances
// if someone leaves the loop then IDs need to be re-negotiated
#define streamframe main
IN streamframe($) {
//  CS srcpath = STREAMSRCDIR "/-" STREAMSRCID ".png";
  CS srcpath = STREAMSRCDIR "/=" STREAMTGTID ".png";
  CS tgtpath = STREAMSRCDIR "/+" STREAMTGTID ".png";
  //CS querystr = getenv("QUERY_STRING");
  //IN querystrlen = strlen(querystr);
  IN framerate = 10;
  IF (1) {
/*
    LORN1s("Content-Type: ", "multipart/x-mixed-replace; boundary=BOUNDARY");
//    LORN1s("Content-Type: ", "video/x-motion-jpeg;boundary=BOUNDARY");
    LORN1s("Cache-Control: ", "max-age=0, must-revalidate");
    LORN1s("Content-Encoding: ", "identity");
    LORN1s("Transfer-Encoding: ", "identity");
    LORN1s("Connection: ", "keep-alive");
    LORN1s("Keep-Alive: ", "timeout=1, max=1");
    LORN1s("Pragma: ", "no-cache");
    LORN("");
      LORN("--BOUNDARY");
*/
//      LOG0;

//    LOG0;
//    LORN("");
//    LORN("--BOUNDARY");
//    LOG0;
    IN sleeptime = 1000000 / framerate;
//    WI (1) { // .....................waits for -0, moves to -1
    IF (1) { // WI (1) for loop .... IF (1) for single frame ...
      FS file = OPENFILE(srcpath); // nope, opens -1
      WI (!file) { // waits for file - appears once fully written (moved -=)
//        WAIT10MS; // 100cps limit -------------------------------------------- HEAVY POLLING
        sched_yield();
        file = OPENFILE(srcpath);
      }
      GOTOEOF(file);
      IN filesize = (IN)ftell(file);
      GOTOSOF(file);

      LORN1s("Content-Type: ", "image/png");
      LORN1i("Content-Length: ", filesize);
      LORN1s("Cache-Control: ", "max-age=0, must-revalidate");
      LORN1s("Content-Encoding: ", "identity");
      LORN1s("Transfer-Encoding: ", "identity");
      LORN1s("Connection: ", "keep-alive");
      LORN1s("Keep-Alive: ", "timeout=5, max=1");
      LORN1s("Pragma: ", "no-cache");
//      LORN1s("Cache-Control: ", "max-age=0, must-revalidate");
//      LORN1s("Pragma: ", "no-cache");
      LORN("");
//      LOG0;
//      CS filedata = (CS)calloc(sizeof(CH), filesize);
      IN sizecount = 0;
      IN inch;
//      LOG("************** FRAME ***************");
      WI (1) {
        inch = GETFCH(file);
        BKEQEOF(inch);
//        IF (INC sizecount LQ filesize)
//          { filedata[sizecount - 1] = inch; }
//        EL { Rc('!'); } ----> send to hellogps as texture <--- stream | hellogps ?
        STDCHOUT(inch);
      }
      LORN("");
      LOG0;
/*
      LORN("--BOUNDARY");
      LOG0;
*/
      CLOSEFILE(file);
      // if receiver has removed previous frame, save this one
      FS nexttarget = EDITFILE(tgtpath);
      IF (!nexttarget) {
        rename(srcpath, tgtpath);
//        IF (rename(srcpath, tgtpath) EQ 0)
//          { Gs("=+"); } EL { Rs("=+!!"); }
      } EL {
        CLOSEFILE(nexttarget);
        remove(srcpath);
//        IF (remove(srcpath) EQ 0)
//          { Gs("=X"); } EL { Rs("=X!!"); }
      }
//      usleep(sleeptime);
    }
  }
/*
  LORN("");
  LORN("--BOUNDARY--");
  LORN("");
  LOG0;
*/
  RT 0;
}
