#include "code.h"

#include <stdio.h>
#include <wchar.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>

#include <signal.h>
#include <errno.h>

#include "lf.h"

#include <time.h>
#include <sys/time.h>
#include "token.h"

#define users main
// raw termios, interrupts deprecated for now - write _ to $@- to close

#define RAMDIR          "/V:"
#define CODESIZE	8
#define TIMESIZE	25
#define NAMESIZE        128
#define SHELLCMD	"shell"
#define READYFRAME	"=1.png"

typedef struct _User {
  IN userid;
  IN latestframeindex;
  CS username;
  CH status;
  IN frameid;
  IN framex, framey;
  IN framew, frameh;
  CS code;
  CS inputbuffer;
  LF fontsize;
  LF mousex, mousey;
// consider dynamic list of lists
// currently just sets as it sees
  struct _User *next;
} User;

User *allusers = NULL;
IN totaluserids = 0; // first user is user 1
IN totalframeids = 1; // first frameid is whole frame
IN latestframeindex = -1; // init frame is 0

FS thisshell = NULL;
VD timetoexit() {
  IF (thisshell)
    { pclose(thisshell); }
  Ys("ENDSHELL");
  KILLPIPEORFAIL("$@-") { Rs("$@- del failed"); }
  KILLPIPEORFAIL("$@+") { Rs("$@+ del failed"); }
  Ls("DELPIPES");
  LOG("\n");
  _exit(0);
}

IN readpipech(INP pipein, INP pipeout) {
  IN ch = EOF;
  CH readbuf[2];
  IN readlen = read(*pipein, readbuf, 1);
  IF (readlen EQ 0) {
    Ys("|..."); //PIPE EOF - WAITING FOR NEW WRITER");
    CLOSEPIPE(*pipeout); // out pipe should be done .. create new
    *pipeout = OPENPIPEWRITE("$@+");
    IF (*pipeout LT 0) { Rs("PIPEOUTREOPENFAILED"); }
    EL { Cs("\n|->"); }
    // new client will open out pipe once in writer is created
//    Gs("REOPENING INWRITER...");
    CLOSEPIPE(*pipein);
    *pipein = OPENPIPEREAD("$@-"); // needs to be an arg ... readpipechwait and readpipechreopen
    IF (*pipein LT 0) { Rs("PIPEINREOPENFAILED"); }
    EL { Gs("|<-"); }
    ch = EOF; // returns an EOF once new pipe writer end is open
    RT ch;
//    Ys("INWAIT");
//    WAIT500MS;
  } EF (readlen LT 0) {
    R1("read error: %d", errno);
    ch = EOF;
    WAIT50MS;
  } EL {
    ch = readbuf[0];
  }
  RT ch;
}

CH writepipech(IN pipe, CH ch) {
  IN writelen = write(pipe, &ch, 1);
  IF (writelen EQ 0) {
    RT EAGAIN; // should be impossible .. writer should wait or fail
//    Rs("WRITEFAIL");
//    RT 0;
  } EF (writelen LT 0) {
    IF (errno EQ EPIPE) // if signal(SIGPIPE, SIG_IGN)
      { RT SIGPIPE; } // expected if reader closes
    EF (errno EQ EPERM) // error 1 looks like success
      { RT EPIPE; } // return error 32 instead
    EL { RT errno; } // return error code > 1
  } EL { RT 1; } // 1 char written
}

CH writepipestr(IN pipe, CS str) {
  WI (str[0] NQNUL) {
    CH wp = writepipech(pipe, str[0]);
    IF (wp NQ 1) { RT wp; } // fail
    INC str;
  }
  RT 1; // 1 string written
}

CH writepipenum(IN pipe, IN num) {
  IN digits = 0;
  IN dignum = num;
  WI (dignum GT 0)
    { dignum DIVS 10; INC digits; }
  IN scale = 1;
  IN scalecount = digits;
  WI (DEC scalecount GT 0) // digits - 1
    { scale MULS 10; }
  WI (scale GQ 1) {
  CH digit = ((num / scale) % 10) + A0;
    CH wp = writepipech(pipe, digit);
    IF (wp NQ 1) { RT wp; } // fail
    scale DIVS 10;
  }
  RT 1; // 1 number written
}

IN users($) {
  CHDIRORFAIL(RAMDIR) { RT1("cannot chdir to ramdir"); }
  // old pipes are removed
  IF ISFILE("$@-") {
Ls("!<-!");
    DELPIPEORFAIL("$@-") { RT1("failed to delete old $@- fi pipe"); }
  }
  IF ISFILE("$@+") {
Ls("!->!");
    DELPIPEORFAIL("$@+") { RT1("failed to delete old $@- fi pipe"); }
  }
  // new pipes are created
  MAKEPIPEORFAIL("$@+") { RT1("make fo pipe fail"); }
  QUICKCMD("chown pbacon:ham \\$@+");
  MAKEPIPEORFAIL("$@-") { RT1("make fi pipe fail"); }
  QUICKCMD("chown pbacon:ham \\$@-");
// ^ else PERMISSION DENIED

  // shell is initialised
  atexit(timetoexit);
  signal(SIGPIPE, SIG_IGN); // don't crash on SIGPIPE
  signal(SIGINT, timetoexit);
  CS initframe = READYFRAME;
  IF ISFILE(initframe) {
    DELFILEORFAIL(initframe)
      { R1("failed to delete initframe %s\n", initframe); }
    // EL { Ls("DELINIT"); }
  } EL { R1("no latestframe %s to del", initframe); }

// should note pid or FS of shell
  FS shell = SENDCMD(SHELLCMD);
  thisshell = shell;
  // shell will generate an initial frame
  FS initframefile = OPENFILE(initframe); // nope, opens -1
  WI (!initframefile) { // waits for file - appears once fully written (moved -=)
    Wc('.');
    WAIT100MS;
    sched_yield();
    initframefile = OPENFILE(initframe);
  }
  GOTOEOF(initframefile);
  IN initframefilesize = (IN)ftell(initframefile);
  GOTOSOF(initframefile);
W1("[%d]", initframefilesize);
  INC latestframeindex;
M1("[%d]", latestframeindex);
  // need to eat the first (0th, 'Connected...') frame
  IN initsizecount = 0;
  WI (INC initsizecount LQ initframefilesize) {
    IN inch = GETFCH(initframefile);
    IFEQEOF(inch) { Rs("initframeEOF"); BK; }
//    EL { writepipech(fo, inch); }
  }
  CLOSEFILE(initframefile);
  // new pipes are connected (waits for first client)
Ys("|...");
  IN fi = OPENPIPEREAD("$@-");
  IF (fi EQ -1) {
    R1("fi: %s\n", strerror(errno));
    RT1("cannot open fi $@-");
  }
Gs("\n|<-");
  IN fo = OPENPIPEWRITE("$@+");
  IF (fo EQ -1) {
    R1("fo: %s\n", strerror(errno));
    RT1("cannot open fo $@+");
  }
Cs("->|");

//  writepipenum(fo, initframefilesize);
//  writepipech(fo, '|');
  // writepipestr(fo, frameusercode);
//  writepipech(fo, '|');
//C2("%d||", initframefilesize);
//D1("[%d>/_]", initsizecount);
//G1("initfilewritten %d", initsizecount);
//Y1("initfiledumped %d", initsizecount);
// FORK INTO BACKGROUND ------------------------?

//                  // if receiver has removed previous frame, save this one
//                  FS nexttarget = EDITFILE(tgtpath);
//                  IF (!nexttarget) {
//  FS mutex = WRITEFILE("$@="); // not necessary since writes to pipe < 512 are atomic (may be 4096)
//  IF (NOT mutex) { RT1("cannot start mutex"); }
//  IF (PUTFCH(mutex, '*') EQEOF) { RT1("mutex init fail"); }
//Ms("READINGPIPE");
  WI (1) {  // read mutex, act on ! (frame users will set the flag and write their frame data)
//    PREVFCH(mutex);
//    IN ch = GETFCH(mutex);
    IN ch = readpipech(&fi, &fo);
// LOGF("\n"); // when all is well, one packet per line
C1("%c", ch); // one packet per line
    IF (ch EQEOF) {
Ms("EOF - PIPE REOPENED");
      CT; // next char
    }
//Yc('.'); WAIT50MS; CT; }
//    IF (ch EQEOF) { RT1("mutex read fail"); }
    EF (ch EQ '_') { BK; } // if client sends _, clean up and close
    EF (ch NQ '|') { // discard anything seen that's not a pipe
      IF INRANGE(ch, ' ', '~') { Rc(ch); }
      EL { R1("(%d)", ch); }
    } EL { // (ch EQ '|')
//    EF (ch EQ '|') { // frames start with | and end with \n
// NOPE -----// frame will write / to mutex and data into fi, return image in fo, frame writes * to mutex
// no mutex ................... write atomic "|code:packet", receive "size|file"
      // start of input
      CH time[TIMESIZE + 1];
      time25str(time, TIMESIZE);
      CH code[TIMESIZE + 1]; // code might be a time
      CS codep = code;
      *codep = NUL;
      CH codehasdots = AN;
      User *thisuser = NULL;
      CS zeroframe = "";
      CS latestframe = READYFRAME;
      CS goodbyeframe = zeroframe;
      CS replyimage = zeroframe;
      WINQEOF(ch) { // don't break on \n .. handle it -- if inner loop leaves an EOF, exit for new packet
      //WINQ2(ch, '\n', EOF) {
//        ch = GETFCH(fif);
        IF (ch NQ ':') // : could be passed from CT and needs to be handled
          { ch = readpipech(&fi, &fo); }
        IF (ch EQEOF) {
           // EOF means the pipe was reopened ... read next char
           Rs(":"); Cs("EOF"); // no : seen
//Ms("EOF - PIPE REOPENED BEFORE :"); // waits here ... BK to continue next packet
           //CT; // get another char
           BK; // get another packet (new pipe = new packet)
        } // EF (ch EQ '\n') {
//Cs("\\n");
          // do not draw for no client
//          Rs("drawing for no client");
//          BK; // : expected ... else no handled code
//        } EL {
//IF (ch NQ ':')
//  { Y1("%c", ch); } // this is a code char
//Cc('.');
//        }
//        IF (ch EQEOF) { RT1("fifo packet notail"); } // no \n
//        IF (ch EQ '\n') { RT1("fifo packet nodata"); } // no :
// treat \n separately - draw with or without an action
        IF (ch EQ ':' OR ch EQ '\n') { // can draw without an action - ch is \n and act will be ...
          // best to pass \n down since it is an action char whereas : implies an action is coming
Cc(':'); // |AbCdEfGh\n will reach here ... |AbCdEfGh:\n behaves the same way
// -------------- MATCH CODE OR LOGIN ------------------------------------------------------------------------------
// might as well read one more char
          IN act = ch; // pass down \n since it implies a draw without an action
          IF (act NQ '\n') // \n needs to be handled this iteration
            { act = readpipech(&fi, &fo); } // expect L, K, M, P, Z, W, V, or \n or EOF - others are invalid action
          IF (act NQEOF AND act NQ '\n') // skip EOF and \n display
            { Yc(act); Mc(':'); }
// consider that other actions may follow
          // \n should return a frame whereas EOF should return a 0 reply
//          IF (act EQEOF) {
//            Gs("PING");
//            Rs("(nopong)");
//            Rs("no action after :");
//            BK; // this packet ended... |AbCdEfGh: = PING - should consider returning a PONG reply <<<<<<<<<<<<
//          } E
          IF (act NQ AL) {
            IF (thisuser EQNULL) { // match code if not already matched (or fail)
              IF (codehasdots EQ AY) {
                Rs("time code with non-login"); // consider the need for relogin with old code and new time
                BK; // cancel this packet
              } EF (codep EQ code) {
                Rs("no code with non-login");
                BK; // cancel this packet
              } EL {
                // find received token in list, update token to new expected
                User *su = allusers;
                WI (su) {
                  IF STREQ(su->code, code)
                    { thisuser = su; BK; }
                  su = su->next;
                }
                IF (thisuser NQNULL) {
                  FPUTF(shell, "\e%d", thisuser->userid);
Yc('{'); Gi(thisuser->userid); Yc(':'); Gs(thisuser->code);  Yc('}');
//                  Y2("%s == (%d)", thisuser->code, thisuser->userid);
//Rs("|^|");
                  IF (act EQEOF) {
                    Gs("ping");
                    replyimage = zeroframe;
                  } EF (thisuser->status EQ 'Z') {
                    Ms("zzzz");
                    replyimage = zeroframe;
                  } EL { replyimage = latestframe; }
// ... once  matched ... rotate token for reply .................................................? !!!!!!!!!!!!!!!!!!!!!111<<<<<<<
                } EL { Rs("usermatch fail"); BK; } // cancel this packet // Ys(code); RT1(" usermatch fail"); }
              } // EL subsequentpacket
            } // thisuser set
          } EL { // (act EQ AL) login
// ----------------------------- LOGIN ACTION -----------------------------------------------------------------
     //       IFEQ(act, 'L') { // ------------ login Lusername-******** -------------------------------------------------
            User *user = (User *)MEM(sizeof(User));
//              user->status = AP; // login mode
//              user->frameid = INC totalframeids; // new frame ... need to submit frame request
            user->userid = INC totaluserids; // user 1, 2, 3.. assume context 1, 2, 3..
            user->latestframeindex = 0; // updates when LT global latestframeindex
            user->username = CHMEM(NAMESIZE + 1);
            CS namep = user->username;
            WINQ2(act, ':', '\n') {
              act = readpipech(&fi, &fo);
//              act = GETFCH(fif);
              IF (act EQEOF) { RT1("fifo username notail"); }
              IF (act EQ '\n') {
                BK; // expect no further commands - should trigger draw
              } EF (act EQ ':') {
                act = readpipech(&fi, &fo);
                IF (act EQEOF) { RT1("fifo next noaction"); }
                BK; // expect another command cycle
              } EL {
                *namep = act;
                // IF (act EQ '.') { namehasdots = AY; }
                INC namep;
                *namep = NUL;
                IF (namep - user->username GQ NAMESIZE)
                  { Rs("username toolong"); act = EOF; ch = EOF; BK; } // should cancel ...
              }
            }
            FPUTF(shell, "\e%d:NAME|%s;", user->userid, user->username); // ; since more params to be updated
            // validate username - associate session
            user->status = AJ; // join immediately for now
            user->frameid = 1; // receive full frame
            user->framex = user->framey = 0; // full frame
            user->framew = user->frameh = 0; // size unknown // could read resulting png size
            user->code = CHMEM(CODESIZE + 1);
            // user can give a timestamp to create a new token or an existing token
            // or token can be created from username and server time - client time is salt
            // server sent request time telling how to rotate / what should return
            // or server time rotated by server time.....
            IF (codep EQ code) { // no code, no salt
              CH newtoken[CODESIZE + 1];
              token8(time, newtoken);
//Y1("%s", newtoken);
              token8rotate(newtoken, time, 1, user->code); // could re-token8
Y1("%s", user->code);
              IF (thisuser EQNULL) { thisuser = user; } // current user
              EL { RT1("logging in to an existing session..."); } // should not happen with no session code
            } EF (codehasdots EQ AY) { // assume this is client time
              CH newtoken1[CODESIZE + 1];
              CH newtoken2[CODESIZE + 1];
//M1("%s", code);
              token8(time, newtoken1);
//Y1("%s", newtoken1);
              token8rotate(newtoken1, time, 1, newtoken2);
//C1("%s", newtoken2);
              token8rotate(newtoken2, code, 1, user->code);
G1("%s", user->code);
              IF (thisuser EQNULL) { thisuser = user; } // current user
              EL { RT1("logging in to an existing session..."); } // should not happen, session code was salt
            } EL { // assume this is an existing token (validate?)
              RT1("fifo packet has token for login (expects time or nothing)");
              // make sure it doesn't correspond to anyone else's token
              // anonymous users do not have sessions at this time ...
              // when they do, this would map to one of those sessions...
              // but when to delete inactive sessions? perhaps only by request
              // request token, send tokenised response ... if slow, timeout
              // slow user is then lost and confused ... better to skip .. send raw
            }
//              FPUTF(shell, "\e%d"); // :CODE=%s;", user->userid, user->code); // do I need to send code to shell? no? might as well tho
            FPUTF(shell, ":CODE|%s;", user->code);
//              user->inputbuffer = CHMEM(INPUTSIZE + 1);
            user->fontsize = (LF){ 32, 0 };
//              FPUTF(shell, "\e%d:FONTSIZE=", user->userid); // A
            FPUTLF(shell, user->fontsize, ":FONTSIZE=", ";");
            user->mousex = (LF){ 0, 0 }; // centre L-R
            user->mousey = (LF){ 0, 0 }; // middle T-B
              //FPUTF(shell, "\e%d:MOUSE.X=", user->userid); // X,Y
              //FPUTLF(shell, user->mousex, "", ",");
              //FPUTLF(shell, user->mousey, "", "\n");
            FPUTLF(shell, user->mousex, ":MOUSE.X=", ";");
            FPUTLF(shell, user->mousey, ":MOUSE.Y=", ";");
            // MOUSE0.X and MOUSE0.Y will be passed through
              // context/data set, ; means ready for next action.. \n later
 // ................check if trailing semicolon is displayed ..... probably .... need CT
            user->next = NULL; // this is the most recent
            IF (allusers) { // if any users
              User *su = allusers; // search to end of
              WI (su->next) { su = su->next; } // list
              su->next = user;  // next (last) in list
            } EL { allusers = user; } // first in list
            thisuser = user; // set current user
            replyimage = latestframe; // user needs a frame
          } // -- end of LOGIN / or MATCH CODE --------------------------------------------------------------------------
// --------------------------- UPDATE COMMANDS ---------- ONLY WORK IF USER IS NOT NULL ---------------------------------
//B1("%c", act); // may include EOF
          IF (!thisuser) { Rs("USER NOT SET"); BK; } // should have broken out earlier
          EL { // IF (thisuser) {
            IFEQ(act, 'F') { // frame Fxywh
              // read X, Y, W, H -> set frame for current user as identified by code
              RT1("frame F");
            } EFEQ(act, 'K') { // write key(s)
//              FPUTF(shell, "\e%d", thisuser->userid); // <---- currently.. assert context each action
              WINQ2(act, ':', '\n') {
                act = readpipech(&fi, &fo);
//                act = GETFCH(fif);
//                IF (act EQEOF) { RT1("key packet EOF"); }
                IF (act EQEOF) { BK; } // return zero frame
                EFEQ2(act, ':', '\n') { BK; } // stop at packet delim : or end \n
                EFEQ(act, '+') { act = ' '; } // + means space
                EFEQ(act, '%') { // % means 2 more bytes expected
                  IN hex1 = readpipech(&fi, &fo);
                  IN hex2 = readpipech(&fi, &fo);
                  act = (HEXCHVALUE(hex1) * 16) + HEXCHVALUE(hex2);
//                  Cc('>'); Yc(act);
                } //EL { Cc('>'); Gc(act); }
//Cc(act);
                IFEQ(act, '\\') {
                  IN ctrl = readpipech(&fi, &fo);
                  IFEQ3(ctrl, 'n', 'b', 't') {
                    IFEQ(ctrl, 'n') { act = '\r'; } // \r replaces \n
                    EFEQ(ctrl, 'b') { act = '\b'; }
                    EFEQ(ctrl, 't') { act = '\t'; }
                    PUTFCH(shell, act);
                  } EFEQ4(ctrl, 'U', 'D', 'L', 'R') {
                    CS actstr = "";
                    IFEQ(ctrl, 'U') { actstr = Kmoveup;    }
                    EFEQ(ctrl, 'D') { actstr = Kmovedown;  }
                    EFEQ(ctrl, 'L') { actstr = Kmoveleft;  }
                    EFEQ(ctrl, 'R') { actstr = Kmoveright; }
//Y1("sending%c", ctrl);
                    PUTFCS(shell, actstr);
                  } EL { R1("Unexpected control char %d", ctrl); }
                } EL {
                  PUTFCH(shell, act);
                }
              } // KAbcdef-G--x
              IFEQ(act, ':') { CT; } // another :action expected
//              EFEQ(act, '\n') { Cs(":KLAST"); }
            } EFEQ(act, 'M') { // set mouse xy
// later consider tagged values .... :M100,200=ROOM+150,200=DOOR;
              CH mmap = An;
              IN mmapi = 0;
              IN mvalx = 0;
              IN mvaly = 0;
              IN mvalz = 0;
              CH mval = AX; // start with X
              CH varval[100];
              IN uvvi = 0;
              varval[uvvi] = NUL;
              WINQ2(act, ':', '\n') {
                IF (act NQ ';') { // CT can trigger next iteration
                  act = readpipech(&fi, &fo);
                } // since ; is also delimeter for last var
//Mc(act);
                IF (act EQEOF) { BK; } // return a zero frame
//Gc(act);
                IF (act EQ ';') {
                  // ; delim used for vars since + can't be distinguished
//Ms("VARS");
                  act = readpipech(&fi, &fo);
                  WINQ4(act, ';', ':', '\n', EOF) {
                    varval[uvvi] = act;
                    varval[INC uvvi] = NUL;
                    act = readpipech(&fi, &fo);
                  } // ;nextvar :Nextcmd \nDraw EOFend
                  FPUTF(shell, ":%s;", varval);
//Ys(varval);
                  fflush(shell);
                  uvvi = 0;
                  varval[uvvi] = NUL;
//Cc(act);
                  IFEQ(act, ';' ) { CT; } // continue this command
                  EFEQ(act, ':' ) { BK; } // next command...
                  EFEQ(act, '\n') { BK; } // draw time...
                  EFEQ(act, EOF ) { BK; } // end without draw..
                } EF (act EQ '+') {
                  IF (mmap EQ An) {
                    // set :Mpreceding+1+1,2
                    IFEQ(mval, AY) {
                      thisuser->mousex.numb = mvalx;
                      thisuser->mousey.numb = mvaly;
                    } EFEQ(mval, AX) { // ignore single value
                      // :M42+1+2,2+3,4++5+6,7 .. ignore 42
                    } EL { Rs("expected XY mval preceding map"); BK; }
//Ms("MOUSE");
                    mvalx = 0;
                    mvaly = 0;
                    mvalz = 0;
                    mval = AX; // ready to receive values once mmap is Ay
                    mmap = Ai; // setting index first
                    mmapi = 0; // should be 0 already the first time
                  } EF (mmap EQ Ai) {
                    // where is the index? do something with it? validate it?
//Cs("INDEX");
                    mmap = Ay; // now receiving values
                  } EFEQ2(mmap, Ay, Ax) { // yes or next
//Ys("VALUE");
                    // next value
                    IF (mval EQ AY) {
// consider focus values ................
//                      FPUTF(shell, ":M1.X
                     // need a special command to trigger lfaddmapfocus, lfaddmapoffset and lfupdatemapshell
                      FPUTF(shell, ":M%d.XVALUE=%d;", mmapi, mvalx); // should add 1 value
                      FPUTF(shell, ":M%d.YVALUE=%d;", mmapi, mvaly); // should add 1 value
       // note last value is added at end of loop
//                      FPUTF(shell, ":MOUSE%d.X=%d;", mmapi, mvalx);
//                      FPUTF(shell, ":MOUSE%d.Y=%d;", mmapi, mvaly);
//              FPUTLF(shell, thisuser->mousex, ":MOUSE.X=", ";");
//              FPUTLF(shell, thisuser->mousey, ":MOUSE.Y=", ";");
                      fflush(shell);
                      mvalx = 0;
                      mvaly = 0;
                      mval = AX; // next value for this index
                    } EL { Rs("expected XY mval"); BK; }
                    mmap = Ax; // next value
                  } EF (mmap EQ Ax) { // :M1,2+1+1,2++2+1,2 (++=xi)
                    Rs("expected one data map only (detected ++)");
                    mmap = Ai; // get next target index
                    mmapi = 0; // second index resets here
                  } EL { R1("mmap = %c", mmap); BK; }
                  CT; // get next key (after +)
                } EFEQ(act, '%') {
                  IN hex1 = readpipech(&fi, &fo);
                  IN hex2 = readpipech(&fi, &fo);
                  act = (HEXCHVALUE(hex1) * 16) + HEXCHVALUE(hex2);
                }
//W1("%c", act);
//                IFEQ(act, ';') {
//                  act = readpipech(&fi, &fo);
//C1("+%c", act);
//                  IF (act NQ ':') {
//                    Rs("expect : or \n after optional ;");
//                    BK;
//                  } EL { Rs("bk.."); BK; }
//                } E
                IFEQ2(act, ':', '\n') { BK; } // stop at packet delim : or term ; or end \n
                EFEQ(act, ',') { // next value element
                  IFEQ(mval, AX) { mval = AY; }
                  EFEQ(mval, AY) { mval = AZ; } // or error ....
                  EL { BK; } // X,Y,Z,stop...
                } EFEQ(act, '+') { // next value set
Rs("unexpected + in mouse data"); BK;
//                  // ADD MOUSE VALUE TO MOUSE PARAM LIST
//                  lfsetlfparam(    // handled above
//                  mval = AX; // regardless of where it was 1; or 1,2,3;
                } EF ISNUMBER(act) { // mmap could be An, Ai, Ay or Ax
                  IFEQ(mmap, Ai) { mmapi = (mmapi * 10) + (act - A0); }
                  EFEQ(mval, AX) { mvalx = (mvalx * 10) + (act - A0); }
                  EFEQ(mval, AY) { mvaly = (mvaly * 10) + (act - A0); }
                  EFEQ(mval, AZ) { mvalz = (mvalz * 10) + (act - A0); }
                } EL { R1("invalid mouse char %d (includes .)", act); BK; } // invalid char - includes .
              }
              IF (mmap EQ An) { // no maps, set value
                thisuser->mousex.numb = mvalx;
                thisuser->mousey.numb = mvaly;
                FPUTLF(shell, thisuser->mousex, ":MOUSE.X=", ";");
                FPUTLF(shell, thisuser->mousey, ":MOUSE.Y=", ";");
                fflush(shell);
              } EL { // set last value
//Ys("LASTVALUE");
                FPUTF(shell, ":M%d.XVALUE=%d;", mmapi, mvalx);
                FPUTF(shell, ":M%d.YVALUE=%d;", mmapi, mvaly);
                fflush(shell);
              }
//Y2("MOUSE %d,%d", mvalx, mvaly);
              IFEQ(act, ':') { CT; } // expect another :action
              // EFEQ(act, '\n') { Cs(":MLAST"); } // else expect draw (or not draw - EOF or something erroneous)
            } EFEQ(act, 'Z') { // start sleeping (send 0 frames .. for time? until event?)
                // reply with 0 frame instead of normal frame
              thisuser->status = 'Z'; // sleeping
              replyimage = zeroframe;
            } EFEQ(act, 'W') { // wake up (give me the newest frame)
              thisuser->status = 'W'; // woke
              replyimage = latestframe;
            } EFEQ(act, 'V') { // leave
              thisuser->status = 'V'; // left
              replyimage = goodbyeframe;
            } EFEQ(act, '0') { // empty frame input
              IF (thisuser->latestframeindex LT latestframeindex)
                { replyimage = latestframe; }
              EL { replyimage = zeroframe; }
              // reply with newest frame or 0 frame if already newest
            } EFEQ(act, '\n') { } // ready for drawing ... handled below
            EFEQ(act, EOF) { } // ready for zero frame .. handled below
            EL { Rs("invalid action"); BK; } //  // RT1("invalid action"); }
    //      } // --- end of LOGIN / UPDATE
//      } END OF IF CH EQ ':' { ---- 
          } // -- end of IF (thisuser) UPDATE options -----------------
          // 
// ---- end of IF (1) --------- if listening to input ------------ // end of prev NQ \n loop (now if 1 - no loop required
//          IFEQ(act, ':') { CT; } // another action expected (BK exits this loop .. need to CT outer loop ...
//          EFEQ(act, ';') { CT; } // loose ; .. something more expected    Rs("loose ; .. missing CT ?"); }
          IFEQ(act, '\n') { // end of packet ---- perhaps after multiple actions ---- now draw
            // update .......... return code and new frame image ... then wait for all others to request an image
            // if they also have updates, their update will be processed and they will receive their image ...
            // subsequent requests will use the new new frame ... meaning not all others must be waited for ... it
            // is an ideal state when all users have the latest frame - need to keep track of frame index
            // if we connect and other users are still replying ..........  cancel, wait....... no, actually update
            IF (thisuser EQNULL) {
              Rs("rogue packet did not login or match session... (should not occur)");
              BK; // cancel this packet ... this error should not occur though
// need to send a blank reply or auth fail image
            } EF (replyimage EQ latestframe) {
              INC latestframeindex;
B1("[%d]", latestframeindex);
              thisuser->latestframeindex = latestframeindex;
              IF ISFILE(latestframe) {
                DELFILEORFAIL(latestframe)
                  { R1("failed to delete latestframe %s\n", latestframe); }
      //          EL { Ls("DELPREV"); } // , latestframe); }
              } EL { R1("no latestframe %s to del", latestframe); }
//F (latestframeindex EQ 1) {
//                    Ys("no previous latestframe");
//                  } EL { R1("previous latestframe %s disappeared", latestframe); }
              FPUTF(shell, "\n"); // linebreak draws new frame
              fflush(shell); // should happen but make sure
// shell what's your status .. did you draw?
//Cxc(96); // diamond each draw
//Gs("DRAW");
//Ms("WAIT");
//Bs("FLUSH");
//M1("waitingfor%s", latestframe);
              FS framefile = OPENFILE(latestframe); // nope, opens -1
              WI (!framefile) { // waits for file - appears once fully written (moved -=)
//                  WAIT10MS; // 100cps limit -------------------------- HEAVY POLLING
Wc('.');
// PUTSTDCH('.'); // one dot per 10ms
WAIT100MS;
                sched_yield();
                framefile = OPENFILE(latestframe);
              }
              GOTOEOF(framefile);
              IN framefilesize = (IN)ftell(framefile);
              GOTOSOF(framefile);
              CS frameusercode = "00000000";
              IF (thisuser)
                { frameusercode = thisuser->code; }
//  M1("latestframe filesize %d for thisuser", framefilesize);
//Ys("writing to fo..");
//  M2("%d|%s|", framefilesize, frameusercode);
//                    FPUTF(fof, "%d|%s|", framefilesize, thisuser->code);
// need to ignore SIGPIPE .. set once earlier
              IF (writepipenum(fo, framefilesize) EQ SIGPIPE) {
        Rs("FO NOT AVAILABLE");
Ms("reconnect..");
        BK; // CT; // next read should trigger a pipe reconnect
              }
              writepipech(fo, '|');
              writepipestr(fo, frameusercode);
              writepipech(fo, '|');
//   C2("%d|%s|", framefilesize, frameusercode);

              IN sizecount = 0;
              WI (INC sizecount LQ framefilesize) {
                IN inch = GETFCH(framefile);
                IFEQEOF(inch) { Rs("unexpected EOF in framefile"); BK; }
                EL { writepipech(fo, inch); }
//                FPUTCH(fof, inch);
//                STDCHOUT(inch);
              }
              CLOSEFILE(framefile);
//G1("filewritten %d", sizecount);
//                  // if receiver has removed previous frame, save this one
//                  FS nexttarget = EDITFILE(tgtpath);
//                  IF (!nexttarget) {
//                    rename(srcpath, tgtpath);
////        IF (rename(srcpath, tgtpath) EQ 0)
////          { Gs("=+"); } EL { Rs("=+!!"); }
//                  } EL {
//                    CLOSEFILE(nexttarget);
//                    remove(srcpath);
////        IF (remove(srcpath) EQ 0)
////          { Gs("=X"); } EL { Rs("=X!!"); }
//      }
// -------------------------------------------------------------------------------------------
            } EF (replyimage EQ zeroframe) { // goodbyeframe is zeroframe
              CS frameusercode = "00000000";
              IF (thisuser)
                { frameusercode = thisuser->code; }
              writepipenum(fo, 0);
              writepipech(fo, '|');
              writepipestr(fo, frameusercode);
              writepipech(fo, '|');
  Y2("%d|%s|", 0, frameusercode);
//                  IF (thisuser) {
//                    FPUTF(fof, "0|%s|", thisuser->code);
//                  } EL { FPUTF(fof, "0|00000000|"); }
            }
//Cs("(done)");
            CT; // same should happen without explicit CT
            // BK; // act was \n .. draw handled ... next input ...
//            CT; // if next char is EOF, it will try to send a 0 frame
//            CT; // next iteration of WINQEOF - could be more commands
            // consider |AbCdEfGh:Ka\n:Kb\n:Kc\n:Kd\n <-- draw draw draw draw a b c d
            //BK; // inner loop gone, outer loop reads for this client ... BK to start again
            //CT; // act was \n... exit inner loop (gone), outer loop should reconnect .. so CT
//            BK; // act should be \n and exit the loop anyway
          //} EL { RT1("packet read fail"); } // chars leftover or nothing read
          } EF (act EQEOF) { // .. end of draw (\n)                      end of :L:U processing ---------------------------------------------------
            // write an empty reply if no draw command was sent ...
            // actually, no reply if no reply was requested
Bs("\\n");
            BK; // will happen at loop condition anyway
          }
            // zero frame handled at EOF in case draw does not occur. zero draw can occur, and is handled above
//            CS frameusercode = "00000000";
//            IF (thisuser)
//              { frameusercode = thisuser->code; }
//            writepipenum(fo, 0);
//            writepipech(fo, '|');
//            writepipestr(fo, frameusercode);
//            writepipech(fo, '|');
//  C2("%d|%s|", 0, frameusercode);
//Cs("(nodraw)");
//            BK; // should happen at loop condition anyway
//            // BK; will happen at next iteration of loop
//          } // end of handle for \n or EOF -- next \n iteration will continue to seek :
        } EL { // code header char
          *codep = ch;
          IF (ch EQ '.') { codehasdots = AY; }
          INC codep;
          *codep = NUL;
          IF (codep - code GQ TIMESIZE) // can read codes up to time length
            { DEC codep; } // overwrite last char
        }
//          IFEQ(act, '\n') // it should
//            { thisuser = NULL; } // could reset user after each input packet - should ?
      } // end WINQ2(ch, '\n', EOF)
      // send any changes to drawshell or redraw everything ... might as well redraw everything
      // inputbuffer, location, mouse - all need to be enabled/disabled
      // input processed
//      IF (PUTFCH(mutex, '/') EQEOF) { RT1("mutex update fail"); }
      // by this point we should have a return image to send in fo
      // send, then wait for frame to update mutex back to * 
      
      // moving to users.c


    } // end of mutex / match
  } // end of mutex loop

//  CLOSECMD(shell); in timetoexit
//  CLOSEFILE(mutex);
//  CLOSEPIPEFILE(fif);
//  CLOSEPIPEFILE(fof);
  timetoexit();
  Gs("users closed (exit should have _exited)\n");
  RT 0;
}
