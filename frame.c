#include "code.h"
#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
//#include <dirent.h>
//#include <time.h>
//#include <sys/time.h>

//#include "strequal.h"
//#include "token.h"

// ---------------------------------------------------
// channel context --- me to you
// breaking [out in(to)] tables
// ----------------------------------------------------

#define RAMDIR          "/V:"
#define MOUSEDATASIZE	50 // 20 + 20 + 10 ish ... enough

VD framelogbase(CS buf, IN nch) {
  CS table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  LOGCH(                   table[  buf[0] >> 2]);
  LOGCH(                   table[((buf[0] & 0x03) << 4) | ((buf[1] & 0xF0) >> 4)]);
  LOGCH((nch LQ 1) ? '=' : table[((buf[1] & 0x0F) << 2) | ((buf[2] & 0xC0) >> 6)]);
  LOGCH((nch LQ 2) ? '=' : table[  buf[2] & 0x3F]);
}

VD frametextheader() {
  LORN1s("Content-Type: ", "text/plain; charset=utf-8");
  LORN1s("Cache-Control: ", "max-age=0, must-revalidate");
  LORN1s("Pragma: ", "no-cache");
  LORN("");
}
VD framehtmlheader() {
  LORN1s("Content-Type: ", "text/html; charset=utf-8");
  LORN1s("Cache-Control: ", "max-age=0, must-revalidate");
  LORN1s("Pragma: ", "no-cache");
  LORN("");
}
      //  LORN1i("Content-Length: ", filesize);
//      LORN1s("Content-Encoding: ", "identity");
//      LORN1s("Transfer-Encoding: ", "identity");
//      LORN1s("Connection: ", "keep-alive");
//      LORN1s("Keep-Alive: ", "timeout=5, max=1");

// auto-increment IDs with multiple instances
// if someone leaves the loop then IDs need to be re-negotiated .. ish
#define UCIWIDTH  100  // code
#define UNIWIDTH  100  // name
#define UKIWIDTH  100  // keys
#define UMIWIDTH  1024000  // mouse ... assume very wide for now
#define UREPLYWIDTH  100  // reply code

#define frame main
IN frame($) {
  //CS querystr = getenv("QUERY_STRING");
  //IN querystrlen = strlen(querystr);
//  IN framerate = 10;
// -------------------- RECV ----------------------------------------------------------------
  CH usercode[UCIWIDTH + 1];
  IN uci = 0;
  usercode[uci] = NUL;
  CH username[UNIWIDTH + 1];
  IN uni = 0;
  username[uni] = NUL;
  CH userkey[UKIWIDTH + 1];
  IN uki = 0;
  userkey[uki] = NUL;
  CH usermouse[UMIWIDTH + 1];
  IN umi = 0;
  usermouse[umi] = NUL;
  CS usermousemap = NULL; // pointer for long packets
  CH replycode[UREPLYWIDTH + 1];
  IN rci = 0;
  replycode[rci] = NUL;
  CH status = AX; // code
  IN lastch = NUL;
  CS contentlen = getenv("CONTENT_LENGTH");
  IN inputlen = -1;
  IF (contentlen) {
    inputlen = 0;
    WI (contentlen[0] NQNUL) {
      inputlen = (inputlen * 10) + (contentlen[0] - A0);
      INC contentlen;
    }
  } // inputlen = INT of contentlen
//Cs("recv");
/*
  IF (0) {
    frametextheader();
    LOGF("INPUTLEN: %d\n", inputlen);

    IN inputj = -1;
    WI (INC inputj LT inputlen) {
      IN ch = GETFCH(stdin);
      IF (ch EQEOF) { LOG("EOF"); BK; }
      IF INRANGE(ch, ' ', '~') {
        LOGF("%c", ch);
      } EL { LOGF("(%d)", ch); }    
    }
    // RT 0;
  }
*/
  CH setmousemap = An; // if a ` is seen, m value will be mapped
  IN mousemapindex = 0; // 0 = default ... MOUSE != MOUSE0
  IN mousefocuscount = 0; // 2 for `123[**10]
  IN mousedatacount = 0; // default is 1 if `123, else n in `123[n]``
  IN usermousecount = -1; // -1 == init index, counts values to assert == mousedatacount
// need one per set ... later
// can send vars and data with `123[n]`` but `` not required (; will trigger)
// if more than one, need to set +123+1,2+3,4++124+1,2+2,3;param=value;param=value
// for now just send one -- need to remember all indexes and counts
  IN inputi = -1;
  WI (inputlen LT 0 OR (INC inputi LT inputlen)) {
    IN ch = GETFCH(stdin);
//Y1(" (%d)", ch);
    IF (ch EQEOF) { BK; }
    EF (ch EQ '\n') { BK; } // no linebreaks expected
//      IF (ch LT 32) { Mi(ch); }
//      EL { Yc(ch); }
    IF (ch EQ '=') {
      IF (lastch EQ Ac) { status = AC; }
      EF (lastch EQ An) { status = AN; }
      EF (lastch EQ Ak) { status = AK; }
      EF (lastch EQ Am) { status = AM; }
      EL { status = AX; }
      CT;
    } EF (ch EQ '&') { status = AX; CT; }
    IF (ch EQ '%') { // expect 2 more chars
      IN hex1 = GETFCH(stdin);
      IN hex2 = GETFCH(stdin);
      ch = (HEXCHVALUE(hex1) * 16) + HEXCHVALUE(hex2);
    } // %20 will be replaced by ' ', etc
//    IF (ch EQ '\n') {
//      IFEQ(status, AC) { status = AN; } // code -> name
//      EFEQ(status, AN) { status = AX; } // name -> done
    IF   (status EQ AC AND uci LT UCIWIDTH) {
      usercode[uci] = ch;
      usercode[INC uci] = NUL;
    } EF (status EQ AN AND uni LT UNIWIDTH) {
      username[uni] = ch;
      username[INC uni] = NUL;
    } EF (status EQ AK AND uki LT UKIWIDTH) {
      IF (ch EQ '`') {
        setmousemap = Ay;
        mousemapindex = 0;
        ch = GETFCH(stdin);
        IF (ch EQ '`') {
          IF (setmousemap EQ Ay) {
            setmousemap = Aw; // data + vars mode (data mode allows vars atm)
          } EL {
            setmousemap = Av; // variable mode
          }
          // in current circumstances this will be the end of the key variable
          // later there may be a size
          WINQ2(ch, '&', EOF) {
            ch = GETFCH(stdin); // or something about `` context (set vars for others ?)
            IF (ch EQ '%') { // expect 2 more chars
              IN hex1 = GETFCH(stdin);
              IN hex2 = GETFCH(stdin);
              ch = (HEXCHVALUE(hex1) * 16) + HEXCHVALUE(hex2);
            } // replace %cc in mousevar string - chars ignored still
Y1("-(%c)", ch); // display ignored chars (could be size)
          }
          IF (ch EQ '&') { status = AX; CT; } // Av means expecting vars in AM stream
          EL {                      // Aw means expecting data then vars in AM stream
            frametextheader();
            LOGF("varmode `` in k but EOF before m");
            LOGF(": %s\n", strerror(errno));
            RT 0;
          }
        } EL { // later: which mousemapindex?
          WI INRANGE(ch, A0, A9) {
            mousemapindex = (mousemapindex * 10) + (ch - A0);
            ch = GETFCH(stdin);
          } // reads 000 then [ 000 ] optionally .. `123[]4 != `1234
          IF (setmousemap EQ An) {
            setmousemap = Ay; // yes, map mode
          } EF (setmousemap EQ Ay) {
            setmousemap = Ay; // no change - could instead count how many maps
          } EF (setmousemap EQ Av OR setmousemap EQ Aw) {
            frametextheader();
            LOGF("data after vars not currently expected");
            LOGF(": %s\n", strerror(errno));
            RT 0;
          }
          mousedatacount = 1; // expect 1 input by default (if no size given)
          IF (ch EQ '%') { // expect 2 more chars
            IN hex1 = GETFCH(stdin);
            IN hex2 = GETFCH(stdin);
            ch = (HEXCHVALUE(hex1) * 16) + HEXCHVALUE(hex2);
          } // after index number, read [ (probably as %nn)
          IF (ch EQ '[') { // or [n] for `2[n]
            // size given - expecting more than one value
            ch = GETFCH(stdin); // might see * as %2A
            IF (ch EQ '%') { // expect 2 more chars
              IN hex1 = GETFCH(stdin);
              IN hex2 = GETFCH(stdin);
              ch = (HEXCHVALUE(hex1) * 16) + HEXCHVALUE(hex2);
            } // after [ , expect * or numbers
            WI (ch EQ '*') { // could be more than one
              INC mousefocuscount; // how many ...
              ch = GETFCH(stdin); // expecting a number
              IF (ch EQ '%') { // expect 2 more chars
                IN hex1 = GETFCH(stdin);
                IN hex2 = GETFCH(stdin);
                ch = (HEXCHVALUE(hex1) * 16) + HEXCHVALUE(hex2);
              } // after ***, read nnn offset count until ]
            } // later: loop *** and nnn but remember each focus count separately
            WI INRANGE(ch, A0, A9) { // later: which mousedatacount?
              mousedatacount = (mousedatacount * 10) + (ch - A0);
              ch = GETFCH(stdin);
            }
            IF (ch EQ '%') { // expect 2 more chars
              IN hex1 = GETFCH(stdin);
              IN hex2 = GETFCH(stdin);
              ch = (HEXCHVALUE(hex1) * 16) + HEXCHVALUE(hex2);
            } // after size number, read ] (probably as %nn)
            IF (ch EQ ']') { // [n has ] .. good
              setmousemap = Ay; // continue.. note that `123[] is safer than `123
               //
            } EL {
              frametextheader();
              LOGF("datamap key len has no ] or invalid char");
              LOGF(": %s\n", strerror(errno));
              RT 0;
            }
          } // `123 expects one m value for 123
        } // `` expects a stream of m actions (ignore size for now but consider later)
      } EL { // no special map key noticed
        userkey[uki] = ch;
        userkey[INC uki] = NUL;
      }
    } EF (status EQ AM) { // AND umi LT 100) {
//      IF (setmousemap EQ Ay) {
      IFEQ2(setmousemap, Ay, Aw) { // data or data + vars
        IF (usermousemap EQNULL) { // first key allocs
          IFNOT(usermousemap = (CS)MEM(MOUSEDATASIZE * mousedatacount) + 1) {
// could assign it to a particular map pointer by index
            frametextheader();
            LOGF("failed to allocate for %d mouse values", mousedatacount);
            LOGF(": %s\n", strerror(errno));
            RT 0; // not negotiable .. exit on failure
          }
        } // then read key
//     // lastch increment indicates size - check == mousedatacount
//   INC usermousecount;
        IF (ch EQ '+') {
          IF (lastch EQ '+') {
            // IF (usermousecount NQ mousedatacount)
             // { Rs("Data size mismatch."); }
            frametextheader();
            LOGF("not expecting multiple datasets yet..");
            LOGF(": %s\n", strerror(errno));
            RT 0; // not negotiable .. exit on failure
          } EL {
            IF (usermousecount EQ -1) {
              // first char is index, check == mousemapindex
              IN chix = GETFCH(stdin);
              IF (chix EQEOF) {
                frametextheader();
                LOGF("EOF, expected mousemapindex after +");
                LOGF(": %s\n", strerror(errno));
                RT 0; // not negotiable .. exit on failure
              } EF INRANGE(chix, A0, A9) {
                IN mmix = chix - A0;
                IF (mmix NQ mousemapindex) {
                  frametextheader();
                  LOGF("mmix and mousemapindex mismatch (%d != %d)", mmix, mousemapindex);
                  LOGF(": %s\n", strerror(errno));
                  RT 0; // not negotiable .. exit on failure
                } EL {
                  IN chdl = GETFCH(stdin);
                  IF (chdl EQEOF) {
                    frametextheader();
                    LOGF("EOF, expected + after matching mousemapindex");
                    LOGF(": %s\n", strerror(errno));
                    RT 0; // not negotiable .. exit on failure
                  }
                  IF (chdl EQ '%') { // expect 2 more chars
                    IN hex1 = GETFCH(stdin);
                    IN hex2 = GETFCH(stdin);
                    chdl = (HEXCHVALUE(hex1) * 16) + HEXCHVALUE(hex2);
                  } // after index number, read + (probably as %2B)
                  IF (chdl NQ '+') {
                    frametextheader();
                    LOGF("expected + after matching mousemapindex (received %d)", chdl);
                    LOGF(": %s\n", strerror(errno));
                    RT 0; // not negotiable .. exit on failure                    
                  } EL { // EL good to go next char (data value)
                    usermousemap[umi] = '+';
// consider indexes > 9 later
                    usermousemap[INC umi] = chix; // mousemapindex + A0
                    usermousemap[INC umi] = '+';
                    usermousemap[INC umi] = NUL;
                  }
                } // end of action for when mousemapindex matches
              } EL {
                frametextheader();
                LOGF("expected numeric mousemapindex after + (received %c)", chix);
                LOGF(": %s\n", strerror(errno));
                RT 0; // not negotiable .. exit on failure
              }
              INC usermousecount; // was -1, now 0 ... need to see first + as non-init
            } EL {
              INC usermousecount; // +1+ 42,23 + <- first INC
              usermousemap[umi] = '+';
              usermousemap[INC umi] = NUL;
            }
          } // end of EL (if lastch NQ '+' - normal inc)
        } EL { // if not a + ...
          usermousemap[umi] = ch;
          usermousemap[INC umi] = NUL;
        }
        // currently pass through everything .. count increments
        // may include variables after ; , will have `` set Aw if valid
        // but currently allows data for either Aw and Ay .. check Ay
        // assert first value is mousemapindex .. then count
        // ++ shifts to next mousemapindex / mousedatacount
      } EF (setmousemap EQ Av) { // use fixed buffer for now but later `` in k may declare a size
        IFEQ(ch, ';') {
          // first + has mouse pos before it.. each have var=val after
          usermouse[umi] = ch;
          usermouse[INC umi] = NUL;
          // currently pass through everything ... use :V instead ? no
          // also need to add usermousecount to count values as received
          // + id + nextval ++ id + nextval ; later add [size] after id
          // would be adding expected size not actual .. use actual
          // size to detect a mismatch -- usermousecount == mousedatacount
        } EL {
          usermouse[umi] = ch;
          usermouse[INC umi] = NUL;
        }
      } EF (umi LT UMIWIDTH) { // else use fixed buffer as default
        usermouse[umi] = ch;
        usermouse[INC umi] = NUL;
      }
    } EL { status = AX; } // BK; }
    lastch = ch;
  } // c=timecode&n=username&k=123!!!

//    RT 0;

// -------------------- HANDLE --------------------------------------------------------------
//Cs("handle");
  CHDIRORFAIL(RAMDIR) { LOG("chdirramdir"); RT 0; }
  IF (ISFILE("$@-") AND ISFILE("$@+")) {
//    IN fi = OPENPIPEWRITE("$@-");
    IN filimit = 15; // 3 * .2 seconds
    IN fi = 0;
    WI (fi LQ 0 AND filimit GT 0) {
//Rs("<");
      fi = open("$@-", O_WRONLY | O_NONBLOCK);
      WAIT200MS; // try every .2 seconds
// or yield?
      DEC filimit;
    }
    IF (fi EQ -1) {
      frametextheader();
      LOGF("cannot open fi $@- for write (3s timeout, no response)");
      LOGF(": %s\n", strerror(errno));
      RT 0;
    }
    IN folimit = 15; // 5 * .2 seconds
    IN fo = 0; // OPENPIPEREAD("$@+");
    WI (fo LQ 0 AND folimit GT 0) {
//Rs(">");
      fo = open("$@+", O_RDONLY | O_NONBLOCK);
      WAIT200MS; // try every .2 seconds
// or yield ?
      DEC folimit;
    }
    IF (fo EQ -1) {
      frametextheader();
      LOG("cannot open fo $@+ for read (3s timeout, server error)");
      LOGF(": %s\n", strerror(errno));
      RT 0;
    } // fo could block forever if server fails to open for write
//Cs("opened");
    // send input data to usermux
    IF (uci GT 0) { // IF (usercode[0] EQNUL) {
//      frametextheader();
//FS ffi = stdout;
      IF (uni GT 0 AND usercode[4] EQ '.') {
//Ys(usercode);
        // users requires datetime as code and name given
        // if name and time given - need to detect 'relogin' if logged in
        write(fi, "|", 1); // wut where did it go
        write(fi, usercode, uci);
        write(fi, ":L", 2);
        write(fi, username, uni);
//        fwrite("|", 1, 1, ffi);
//        fwrite(usercode, 1, uci, ffi);
//        fwrite(":L", 1, 2, ffi);
//        fwrite(username, 1, uni, ffi);
      } EL {
//Ms(usercode);
        // this is presumed to be a valid code (or name not given - login failure time should be logged if so)
        // if name is given with a code, name is currently ignored
        write(fi, "|", 1);
        write(fi, usercode, uci);
//        fwrite("|", 1, 1, ffi);
//        fwrite(usercode, 1, uci, ffi);
      }
      IF (uki GT 0) {
        write(fi, ":K", 2);
        write(fi, userkey, uki);
//        fwrite(":K", 1, 2, ffi);
//        fwrite(userkey, 1, uki, ffi);
      }
      IF (umi GT 0) {
        write(fi, ":M", 2);
        IF (setmousemap EQ Ay) {
          write(fi, usermousemap, umi);
          // does not indicate more than one value ..
          // can't use ; to delimit since has meaning ....
          // m= passed through as M ... later can add size from k=`n[m]
          // M1,2+2+-3,4+-1,-1+-4,8+1,2+3,4 // first is index
        } EF (setmousemap EQ Av) {
          write(fi, usermouse, umi); // pass through as-is at the moment - may use different buffer later
        } EL {
          write(fi, usermouse, umi);
        }
//        fwrite(":M", 1, 2, ffi);
//        fwrite(usermouse, 1, umi, ffi);
      }
//      IF (upi GT 0) {
//        write(fi, ":P", 2);
//        write(fi, userpos, upi);
//        fwrite(":P", 1, 2, ffi);
//        fwrite(userpos, 1, upi, ffi);
//      }
      write(fi, "\n", 1); //Ms("|2018.10.10:Luser:Ktest\\n >> fi");
//      fwrite("\n", 1, 1, ffi); //Ms("|2018.10.10:Luser:Ktest\\n >> fi");
//fflush(ffi);
//LOG("reply data ignored");
//RT 0;
    } EL {
      frametextheader();
      LOG("usercode required; no data sent\n");
      RT 0;
    }
    // expect an atomic reply, but it may not be immediate
    IN readbytes = 0;
    CH readbuf[2]; // extra for padding
    WI (readbytes EQ 0) {
//Cs("FO");
      readbytes = read(fo, readbuf, 1);
      IF (readbytes EQ 0) {
//        Ws("waiting..");
        WAIT100MS; // expect EAGAIN -- should BK
        BK;
      } EF (readbytes LT 0) {
        IF (errno EQ EAGAIN) {
          readbytes = 0; // non-blocking, try again
          sched_yield(); // give processor some time
        } EL { BK; }
      } // EL { Gs("PIPEREPLY"); }
    } // current not breaking on errors
    IF (readbytes LT 0) {
      frametextheader();
      LOGF("failed to read fo (error %d)\n", errno);
      LOGF("%s\n", strerror(errno));
      RT 0;
    } EF (readbytes EQ 0) {
      frametextheader();
      LOGF("fo returned 0 bytes in nonblocking read A\n");
      LOGF("(error %d) %s\n", errno, strerror(errno));
      RT 0;
    } EF (readbytes GT 0) { // should be equal to 1
      IN countbuf = (readbuf[0] - A0);
      WI (readbuf[0] NQ '|') { // ==============================
        readbytes = read(fo, readbuf, 1); // <<<<<<<<<<<<<<<<<<<
        IF (readbytes EQ 0) { // -------------------------------
 //         Ys("COUNTWAIT");
          WAIT100MS; // expect EAGAIN -- should BK
          BK;
        } EF (readbytes LT 0) {
          IF (errno EQ EAGAIN) {
            readbytes = 0; // non-blocking, try again
            sched_yield(); // give processor some time
          } EL { BK; } // bad error
 //         R1("READCOUNTERR: %d", errno);
//          WAIT500MS;
        } EF INRANGE(readbuf[0], A0, A9) { // ------------------
          countbuf = (countbuf * 10) + (readbuf[0] - A0);
        } EF (readbuf[0] EQ '|') { // --------------------------
//          G1("expecting %d bytes", countbuf);
          BK;
        } EL { BK; } // { R1("expected N or |, not %d", readbuf[0]); BK; }
      } // =====================================================
  // need other properties for reply ... new code, username
      IF (readbytes LT 0) {
        frametextheader();
        LOGF("failed to read fo count (error %d)\n", errno);
        LOGF("%s\n", strerror(errno));
        RT 0;
      } EF (readbytes EQ 0) {
        frametextheader();
        LOGF("fo returned 0 bytes in nonblocking read B\n");
        LOGF("(error %d) %s\n", errno, strerror(errno));
        RT 0;
      } EF (readbuf[0] EQ '|') {
        readbuf[0] = NUL;
        WI (readbuf[0] NQ '|') {
          readbytes = read(fo, readbuf, 1);
          IF (readbytes EQ 0) {
//            Ys("CODEWAIT");
            WAIT500MS; // expect EAGAIN -- should BK
            BK;
          } EF (readbytes LT 0) {
            IF (errno EQ EAGAIN) {
              readbytes = 0; // non-blocking, try again
              sched_yield(); // give processor some time
            } EL { BK; } // bad error
//            R1("READCODEERR: %d", errno);
//            WAIT500MS;
          } EF (readbuf[0] EQ '|') {
//            Cs("coderead");
            BK;
          } EF INRANGE(readbuf[0], ' ', '~') {
            replycode[rci] = readbuf[0];
            replycode[INC rci] = NUL;
          } EL { BK; } // Rs("expected C or |"); BK; }
        }
      } // read code first
      IF (readbytes LT 0) {
        frametextheader();
        LOGF("failed to read fo code (error %d)\n", errno);
        LOGF("%s\n", strerror(errno));
        RT 0;
      } EF (readbytes EQ 0) {
        frametextheader();
        LOGF("fo returned 0 bytes in nonblocking read C\n");
        LOGF("(error %d) %s\n", errno, strerror(errno));
        RT 0;
      } EF (readbuf[0] NQ '|') {
        frametextheader();
        LOGF("failed to read fo data delim (error %d)\n", errno);
        LOGF("%s\n", strerror(errno));
        RT 0;
      } EL {
        // ready to stream data
        framehtmlheader();
        LOGF("<html><head>\n");
        LOGF("  <script type=\"text/javascript\">\n");
        LOGF("    function send() {\n");
        LOGF("      try {\n");
        LOGF("        window.parent.postMessage(document.body.innerHTML, '*');\n");
        LOGF("        document.body.innerHTML = 'Data sent.';\n");
        LOGF("        document.body.style.display = '';\n");
        LOGF("      } catch (e) { console.log('failed to send message...'); console.log(e); }\n");
        LOGF("    }\n");
        LOGF("  </script>\n");
        LOGF("</head><body style=\"display: none; word-wrap: break-word;\" onload=\"send();\">{ \n");
        LOGF("\"code\": \"%s\",\n", replycode);
         // ...file will be streamed
        LOGF("\"data\": \"");
        IN bytesread = 0; // up to 65535 should be possible
        CH basebuf[3]; // base64 uses triplets to make quads
        IN bbi = 0; // 0, 1, 2 -> convert. tail -> +1== or ++2=
        WI (bytesread LT countbuf) {
          readbytes = read(fo, readbuf, 1);
          IF (readbytes EQ 0) {
//            Ys("DATAWAIT");
            WAIT50MS; // expect EAGAIN -- should BK
            sched_yield();
            BK; // should check to see if this happens .... it should't
          } EF (readbytes LT 0) {
            IF (errno EQ EAGAIN) {
              readbytes = 0; // non-blocking, try again
              sched_yield(); // give processor some time
            } EL { BK; } // bad error
//            R1("READDATAERR: %d", errno);
//            WAIT500MS;
          } EL {
            // base64 requires 3 chars at a time
            IF (bbi EQ 0 OR bbi EQ 1) {
              basebuf[bbi] = readbuf[0];
              INC bbi;
            } EF (bbi EQ 2) {
              basebuf[bbi] = readbuf[0];
              framelogbase(basebuf, 3);
              bbi = 0;
            }
            INC bytesread;
          }
        }
        IF (readbytes EQ 0) {
          LOGF("- 0 bytes in non-blocking read D-");
        } EF (readbytes LT 0) // == 0 will be stuck in loop
          { LOGF("- read error -"); }
        EF (bbi GT 0) // part of a triplet remains
          { framelogbase(basebuf, bbi); }
        LOGF("\",\n");
//LOGF("INPUTI:%d,INPUTLEN:%d,\n", inputi, inputlen);
        LOGF("\"dlen\": %d }</body></html>\n", bytesread);
      } // ---- end of sucessful frame reply
    } EL {
      frametextheader();
      LOG("readbytesLQ0,EOFin\n");
    }
  } EL {
    frametextheader();
    LOG("pipes not available\n");
  }
//      CS filedata = (CS)calloc(sizeof(CH), filesize);
  LOG0;
  RT 0;
}
