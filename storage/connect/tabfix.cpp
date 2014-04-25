/************* TabFix C++ Program Source Code File (.CPP) **************/
/* PROGRAM NAME: TABFIX                                                */
/* -------------                                                       */
/*  Version 4.9                                                        */
/*                                                                     */
/* COPYRIGHT:                                                          */
/* ----------                                                          */
/*  (C) Copyright to the author Olivier BERTRAND          1998-2014    */
/*                                                                     */
/* WHAT THIS PROGRAM DOES:                                             */
/* -----------------------                                             */
/*  This program are the TDBFIX class DB routines.                     */
/*                                                                     */
/***********************************************************************/

/***********************************************************************/
/*  Include relevant section of system dependant header files.         */
/***********************************************************************/
#include "my_global.h"
#if defined(WIN32)
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#if defined(__BORLANDC__)
#define __MFC_COMPAT__                   // To define min/max as macro
#endif   // __BORLANDC__
//#include <windows.h>
#else   // !WIN32
#if defined(UNIX)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#else   // !UNIX
#include <io.h>
#endif  // !UNIX
#include <fcntl.h>
#endif  // !WIN32

/***********************************************************************/
/*  Include application header files:                                  */
/***********************************************************************/
#include "global.h"      // global declares
#include "plgdbsem.h"    // DB application declares
#include "filamfix.h"
#include "filamdbf.h"
#include "tabfix.h"      // TDBFIX, FIXCOL classes declares

/***********************************************************************/
/*  DB static variables.                                               */
/***********************************************************************/
extern "C" int trace;
extern int num_read, num_there, num_eq[2];               // Statistics
static const longlong M2G = 0x80000000;
static const longlong M4G = (longlong)2 * M2G;

/* ------------------------------------------------------------------- */

/***********************************************************************/
/*  Implementation of the TDBFIX class.                                */
/***********************************************************************/
TDBFIX::TDBFIX(PDOSDEF tdp, PTXF txfp) : TDBDOS(tdp, txfp)
  {
//Cardinal = -1;
  } // end of TDBFIX standard constructor

TDBFIX::TDBFIX(PGLOBAL g, PTDBFIX tdbp) : TDBDOS(g, tdbp)
  {
//Cardinal = tdbp->Cardinal;
  } // end of TDBFIX copy constructor

// Method
PTDB TDBFIX::CopyOne(PTABS t)
  {
  PTDB    tp;
  PGLOBAL g = t->G;

  tp = new(g) TDBFIX(g, this);

  if (Ftype < 2) {
    // File is text
    PDOSCOL cp1, cp2;

    for (cp1 = (PDOSCOL)Columns; cp1; cp1 = (PDOSCOL)cp1->GetNext()) {
      cp2 = new(g) DOSCOL(cp1, tp);  // Make a copy
      NewPointer(t, cp1, cp2);
      } // endfor cp1

  } else {
    // File is binary
    PBINCOL cp1, cp2;

    for (cp1 = (PBINCOL)Columns; cp1; cp1 = (PBINCOL)cp1->GetNext()) {
      cp2 = new(g) BINCOL(cp1, tp);  // Make a copy
      NewPointer(t, cp1, cp2);
      } // endfor cp1

  } // endif Ftype

  return tp;
  } // end of CopyOne

/***********************************************************************/
/*  Reset read/write position values.                                  */
/***********************************************************************/
void TDBFIX::ResetDB(void)
  {
  TDBDOS::ResetDB();
  } // end of ResetDB

/***********************************************************************/
/*  Allocate FIX (DOS) or BIN column description block.                */
/***********************************************************************/
PCOL TDBFIX::MakeCol(PGLOBAL g, PCOLDEF cdp, PCOL cprec, int n)
  {
  if (Ftype == RECFM_BIN)
    return new(g) BINCOL(g, cdp, this, cprec, n);
  else
    return new(g) DOSCOL(g, cdp, this, cprec, n);

  } // end of MakeCol

/***********************************************************************/
/*  Remake the indexes after the table was modified.                   */
/***********************************************************************/
int TDBFIX::ResetTableOpt(PGLOBAL g, bool dox)
  {
  RestoreNrec();                        // May have been modified
  return TDBDOS::ResetTableOpt(g, dox);
  } // end of ResetTableOpt

/***********************************************************************/
/*  Reset the Nrec and BlkSize values that can have been modified.     */
/***********************************************************************/
void TDBFIX::RestoreNrec(void)
  {
  if (!Txfp->Padded) {
    Txfp->Nrec = (To_Def && To_Def->GetElemt()) ? To_Def->GetElemt()
                                                : DOS_BUFF_LEN;
    Txfp->Blksize = Txfp->Nrec * Txfp->Lrecl;
    } // endif Padded

  } // end of RestoreNrec

/***********************************************************************/
/*  FIX Cardinality: returns table cardinality in number of rows.      */
/*  This function can be called with a null argument to test the       */
/*  availability of Cardinality implementation (1 yes, 0 no).          */
/***********************************************************************/
int TDBFIX::Cardinality(PGLOBAL g)
  {
  if (!g)
    return Txfp->Cardinality(g);

  if (Cardinal < 0)
    Cardinal = Txfp->Cardinality(g);

  return Cardinal;
  } // end of Cardinality

/***********************************************************************/
/*  FIX GetMaxSize: returns file size in number of lines.              */
/***********************************************************************/
int TDBFIX::GetMaxSize(PGLOBAL g)
  {
  if (MaxSize < 0)
    MaxSize = Cardinality(g);

  return MaxSize;
  } // end of GetMaxSize

/***********************************************************************/
/*  FIX ResetSize: Must reset Headlen for DBF tables only.             */
/***********************************************************************/
void TDBFIX::ResetSize(void)
  {
  if (Txfp->GetAmType() == TYPE_AM_DBF)
    Txfp->Headlen = 0;

  MaxSize = Cardinal = -1;
  } // end of ResetSize

/***********************************************************************/
/*  FIX GetProgMax: get the max value for progress information.        */
/***********************************************************************/
int TDBFIX::GetProgMax(PGLOBAL g)
  {
  return Cardinality(g);
  } // end of GetProgMax

/***********************************************************************/
/*  RowNumber: return the ordinal number of the current row.           */
/***********************************************************************/
int TDBFIX::RowNumber(PGLOBAL g, bool b)
  {
  if (Txfp->GetAmType() == TYPE_AM_DBF) {
    if (!b && To_Kindex) {
      /*****************************************************************/
      /*  Don't know how to retrieve Rows from DBF file address        */
      /*  because of eventual deleted lines still in the file.         */
      /*****************************************************************/
      sprintf(g->Message, MSG(NO_ROWID_FOR_AM),
                          GetAmName(g, Txfp->GetAmType()));
      return 0;
      } // endif To_Kindex

    if (!b)
      return Txfp->GetRows();

    } // endif DBF

  return Txfp->GetRowID();
  } // end of RowNumber

/***********************************************************************/
/*  FIX tables don't use temporary files except if specified as do it. */
/***********************************************************************/
bool TDBFIX::IsUsingTemp(PGLOBAL g)
  {
  USETEMP usetemp = PlgGetUser(g)->UseTemp;

  return (usetemp == TMP_YES || usetemp == TMP_FORCE);
  } // end of IsUsingTemp

/***********************************************************************/
/*  FIX Access Method opening routine (also used by the BIN a.m.)      */
/*  New method now that this routine is called recursively (last table */
/*  first in reverse order): index blocks are immediately linked to    */
/*  join block of next table if it exists or else are discarted.       */
/***********************************************************************/
bool TDBFIX::OpenDB(PGLOBAL g)
  {
  if (trace)
    htrc("FIX OpenDB: tdbp=%p tdb=R%d use=%d key=%p mode=%d Ftype=%d\n",
      this, Tdb_No, Use, To_Key_Col, Mode, Ftype);

  if (Use == USE_OPEN) {
    /*******************************************************************/
    /*  Table already open, just replace it at its beginning.          */
    /*******************************************************************/
    if (To_Kindex)
      /*****************************************************************/
      /*  Table is to be accessed through a sorted index table.        */
      /*****************************************************************/
      To_Kindex->Reset();
    else
      Txfp->Rewind();       // see comment in Work.log

    return false;
    } // endif use

  if (Mode == MODE_DELETE && !Next && Txfp->GetAmType() == TYPE_AM_MAP) {
    // Delete all lines. Not handled in MAP mode
    Txfp = new(g) FIXFAM((PDOSDEF)To_Def);
    Txfp->SetTdbp(this);
    } // endif Mode

  /*********************************************************************/
  /*  Call Cardinality to calculate Block in the case of Func queries. */
  /*  and also in the case of multiple tables.                         */
  /*********************************************************************/
  if (Cardinality(g) < 0)
    return true;           

  /*********************************************************************/
  /*  Open according to required logical input/output mode.            */
  /*  Use conventionnal input/output functions.                        */
  /*  Treat fixed length text files as binary.                         */
  /*********************************************************************/
  if (Txfp->OpenTableFile(g))
    return true;

  Use = USE_OPEN;       // Do it now in case we are recursively called

  /*********************************************************************/
  /*  Initialize To_Line at the beginning of the block buffer.         */
  /*********************************************************************/
  To_Line = Txfp->GetBuf();                       // For WriteDB

  if (trace)
    htrc("OpenDos: R%hd mode=%d\n", Tdb_No, Mode);

  /*********************************************************************/
  /*  Reset buffer access according to indexing and to mode.           */
  /*********************************************************************/
  Txfp->ResetBuffer(g);

  /*********************************************************************/
  /*  Reset statistics values.                                         */
  /*********************************************************************/
  num_read = num_there = num_eq[0] = num_eq[1] = 0;
  return false;
  } // end of OpenDB

/***********************************************************************/
/*  WriteDB: Data Base write routine for FIX access method.            */
/***********************************************************************/
int TDBFIX::WriteDB(PGLOBAL g)
  {
  return Txfp->WriteBuffer(g);
  } // end of WriteDB

// ------------------------ BINCOL functions ----------------------------

/***********************************************************************/
/*  BINCOL public constructor.                                         */
/***********************************************************************/
BINCOL::BINCOL(PGLOBAL g, PCOLDEF cdp, PTDB tp, PCOL cp, int i, PSZ am)
  : DOSCOL(g, cdp, tp, cp, i, am)
  {
  Fmt = (cdp->GetFmt()) ? toupper(*cdp->GetFmt()) : 'X';
  } // end of BINCOL constructor

/***********************************************************************/
/*  FIXCOL constructor used for copying columns.                       */
/*  tdbp is the pointer to the new table descriptor.                   */
/***********************************************************************/
BINCOL::BINCOL(BINCOL *col1, PTDB tdbp) : DOSCOL(col1, tdbp)
  {
  Fmt = col1->Fmt;
  } // end of BINCOL copy constructor

/***********************************************************************/
/*  ReadColumn: what this routine does is to access the last line      */
/*  read from the corresponding table and extract from it the field    */
/*  corresponding to this column.                                      */
/***********************************************************************/
void BINCOL::ReadColumn(PGLOBAL g)
  {
  char   *p = NULL;
  int     rc;
  PTDBFIX tdbp = (PTDBFIX)To_Tdb;

  if (trace > 1)
    htrc("BIN ReadColumn: col %s R%d coluse=%.4X status=%.4X buf_type=%d\n",
      Name, tdbp->GetTdb_No(), ColUse, Status, Buf_Type);

  /*********************************************************************/
  /*  If physical reading of the line was deferred, do it now.         */
  /*********************************************************************/
  if (!tdbp->IsRead())
    if ((rc = tdbp->ReadBuffer(g)) != RC_OK) {
      if (rc == RC_EF)
        sprintf(g->Message, MSG(INV_DEF_READ), rc);

      longjmp(g->jumper[g->jump_level], 11);
      } // endif

  p = tdbp->To_Line + Deplac;

  /*********************************************************************/
  /*  Set Value from the line field.                                   */
  /*********************************************************************/
  switch (Fmt) {
    case 'X':                 // Standard not converted values
      Value->SetBinValue(p);
      break;
    case 'S':                 // Short integer
      Value->SetValue((int)*(short*)p);
      break;
    case 'T':                 // Tiny integer
      Value->SetValue((int)*p);
      break;
    case 'L':                 // Long Integer
      strcpy(g->Message, "Format L is deprecated, use I");
      longjmp(g->jumper[g->jump_level], 11);
    case 'I':                 // Integer
      Value->SetValue(*(int*)p);
      break;
    case 'F':                 // Float
    case 'R':                 // Real
      Value->SetValue((double)*(float*)p);
      break;
    case 'D':                 // Double
      Value->SetValue(*(double*)p);
      break;
    case 'C':                 // Text
      if (Value->SetValue_char(p, Long)) {
        sprintf(g->Message, "Out of range value for column %s at row %d",
                Name, tdbp->RowNumber(g));
        PushWarning(g, tdbp);
        } // endif SetValue_char

      break;
    default:
      sprintf(g->Message, MSG(BAD_BIN_FMT), Fmt, Name);
      longjmp(g->jumper[g->jump_level], 11);
      } // endswitch Fmt

  // Set null when applicable
  if (Nullable)
    Value->SetNull(Value->IsZero());

  } // end of ReadColumn

/***********************************************************************/
/*  WriteColumn: what this routine does is to access the last line     */
/*  read from the corresponding table, and rewrite the field           */
/*  corresponding to this column from the column buffer.               */
/***********************************************************************/
void BINCOL::WriteColumn(PGLOBAL g)
  {
  char    *p, *s;
  longlong n;
  PTDBFIX  tdbp = (PTDBFIX)To_Tdb;

  if (trace) {
    htrc("BIN WriteColumn: col %s R%d coluse=%.4X status=%.4X",
          Name, tdbp->GetTdb_No(), ColUse, Status);
    htrc(" Lrecl=%d\n", tdbp->Lrecl);
    htrc("Long=%d deplac=%d coltype=%d ftype=%c\n",
          Long, Deplac, Buf_Type, *Format.Type);
    } // endif trace

  /*********************************************************************/
  /*  Check whether the new value has to be converted to Buf_Type.     */
  /*********************************************************************/
  if (Value != To_Val)
    Value->SetValue_pval(To_Val, false);    // Convert the updated value

  p = tdbp->To_Line + Deplac;

  /*********************************************************************/
  /*  Check whether updating is Ok, meaning col value is not too long. */
  /*  Updating will be done only during the second pass (Status=true)  */
  /*  Conversion occurs if the external format Fmt is specified.       */
  /*********************************************************************/
  switch (Fmt) {
    case 'X':
      // Standard not converted values
      if (Value->GetBinValue(p, Long, Status)) {
        sprintf(g->Message, MSG(BIN_F_TOO_LONG),
                            Name, Value->GetSize(), Long);
        longjmp(g->jumper[g->jump_level], 31);
        } // endif Fmt

      break;
    case 'S':                 // Short integer
      n = Value->GetBigintValue();

      if (n > 32767LL || n < -32768LL) {
        sprintf(g->Message, MSG(VALUE_TOO_BIG), n, Name);
        longjmp(g->jumper[g->jump_level], 31);
      } else if (Status)
        *(short *)p = (short)n;

      break;
    case 'T':                 // Tiny integer
      n = Value->GetBigintValue();

      if (n > 255LL || n < -256LL) {
        sprintf(g->Message, MSG(VALUE_TOO_BIG), n, Name);
        longjmp(g->jumper[g->jump_level], 31);
      } else if (Status)
        *p = (char)n;

      break;
    case 'L':                 // Long Integer
      strcpy(g->Message, "Format L is deprecated, use I");
      longjmp(g->jumper[g->jump_level], 11);
    case 'I':                 // Integer
      n = Value->GetBigintValue();

      if (n > INT_MAX || n < INT_MIN) {
        sprintf(g->Message, MSG(VALUE_TOO_BIG), n, Name);
        longjmp(g->jumper[g->jump_level], 31);
      } else if (Status)
        *(int *)p = Value->GetIntValue();

      break;
    case 'B':                 // Large (big) integer
      if (Status)
        *(longlong *)p = (longlong)Value->GetBigintValue();

      break;
    case 'F':                 // Float
    case 'R':                 // Real
      if (Status)
        *(float *)p = (float)Value->GetFloatValue();

      break;
    case 'D':                 // Double
      if (Status)
        *(double *)p = Value->GetFloatValue();

      break;
    case 'C':                 // Characters
      if ((n = (signed)strlen(Value->GetCharString(Buf))) > Long) {
        sprintf(g->Message, MSG(BIN_F_TOO_LONG), Name, (int) n, Long);
        longjmp(g->jumper[g->jump_level], 31);
        } // endif n

      if (Status) {
        s = Value->GetCharString(Buf);
        memset(p, ' ', Long);
        memcpy(p, s, strlen(s));
        } // endif Status

      break;
    default:
      sprintf(g->Message, MSG(BAD_BIN_FMT), Fmt, Name);
      longjmp(g->jumper[g->jump_level], 11);
    } // endswitch Fmt

  } // end of WriteColumn

/* ------------------------ End of TabFix ---------------------------- */
