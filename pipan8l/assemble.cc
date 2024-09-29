//    Copyright (C) Mike Rieker, Beverly, MA USA
//    www.outerworldapps.com
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; version 2 of the License.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    EXPECT it to FAIL when someone's HeALTh or PROpeRTy is at RISk.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//    http://www.gnu.org/licenses/gpl-2.0.html

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assemble.h"

#define TT_ENDLINE 0
#define TT_INTEGER 1
#define TT_SYMBOL 2
#define TT_DELIM 3
#define TT_STRING 4

#define ST_NORMAL 0
#define ST_PMEMOP 1

struct Symbol {
    Symbol *next;           // next in symbols list
    uint16_t value;         // value
    char defind;            // set if defined
    char global;            // set if global
    char hidden;            // set to hide from symbol table listing
    char symtyp;            // symbol type ST_*
    char name[1];           // null terminated symbol name
};

struct Token {
    Token *next;            // next token in line
    char const *srcloc;     // source string pointer
    uint32_t intval;        // integer value
    char toktype;           // TT_ token type
    char strval[1];         // string value (null terminated)
};

static char const *const pmemops[] = { "AND",  "TAD",  "ISZ",  "DCA",  "JMS",  "JMP"  };
static char const *const pindops[] = { "ANDI", "TADI", "ISZI", "DCAI", "JMSI", "JMPI" };

static Symbol *symbols;

static Symbol *defopsym (Symbol *lastsym, char symtyp, uint16_t opc, char const *mne);
static char *tokenize (char const *srcname, Token **token_r);
static char *decodeopcode (Token *token, uint16_t address, uint16_t *opcode_r);
static char *decomicro (Token *token, uint16_t *opcode_r);
static char const *microconflict (uint64_t already, uint64_t adding);
static char *mprintf (char const *fmt, ...);


// assemble the given instruction
//  input:
//   opstr = null-terminated instruction string
//   address = address that the instruction goes at
//  output:
//   returns NULL: success, *opcode_r = opcode
//           else: error message (must be freed)
char *assemble (char const *opstr, uint16_t address, uint16_t *opcode_r)
{
    if (symbols == NULL) {
        for (int i = 0; i < (int) (sizeof pmemops / sizeof pmemops[0]); i ++) {
            symbols = defopsym (symbols, ST_PMEMOP, i * 01000, pmemops[i]);
            symbols = defopsym (symbols, ST_PMEMOP, i * 01000 + 00400, pindops[i]);
        }
    }

    Token *tokens = NULL;
    char *err = tokenize (opstr, &tokens);
    if (err == NULL) err = decodeopcode (tokens, address, opcode_r);
    for (Token *token; (token = tokens) != NULL;) {
        tokens = token->next;
        free (token);
    }
    return err;
}

static Symbol *defopsym (Symbol *lastsym, char symtyp, uint16_t opc, char const *mne)
{
    Symbol *symbol = (Symbol *) malloc (strlen (mne) + sizeof *symbol);
    symbol->next   = lastsym;
    symbol->value  = opc;
    symbol->defind = 1;
    symbol->global = 0;
    symbol->hidden = 1;
    symbol->symtyp = symtyp;
    strcpy (symbol->name, mne);
    return symbol;
}

static char *tokenize (char const *asmline, Token **token_r)
{
    char const *q;
    char c, *p;
    int i;
    Token **ltoken, *token;
    uint32_t v;

    q = asmline;
    ltoken = token_r;
    while ((c = *q) != 0) {

        // skip whitespace
        if (c <= ' ') {
            ++ q;
            continue;
        }

        // numeric constant
        if ((c >= '0') && (c <= '9')) {
            v = strtoul (q, &p, 0);
            i = (char const *) p - q;
            token = (Token *) malloc (i + sizeof *token);
            token->next = NULL;
            token->srcloc = q;
            token->toktype = TT_INTEGER;
            token->intval = v;
            memcpy (token->strval, q, i);
            token->strval[i] = 0;
            q = p;
            *ltoken = token;
            ltoken = &token->next;
            continue;
        }

        // symbol
        if (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || (c == '_') || (c == '$') || (c == '.')) {
            for (i = 0;; i ++) {
                c = q[i];
                if ((c >= 'a') && (c <= 'z')) continue;
                if ((c >= 'A') && (c <= 'Z')) continue;
                if ((c >= '0') && (c <= '9')) continue;
                if (c == '_') continue;
                if (c == '$') continue;
                if (c == '.') continue;
                if ((c == ':') && (q[i+1] == ':')) {
                    i ++;
                    continue;
                }
                break;
            }
            token = (Token *) malloc (i + sizeof *token);
            token->next = NULL;
            token->srcloc = q;
            token->toktype = TT_SYMBOL;
            token->intval = i;
            memcpy (token->strval, q, i);
            token->strval[i] = 0;
            q += i;
            *ltoken = token;
            ltoken = &token->next;
            continue;
        }

        // delimiter
        if (strchr ("@+-&|^~*/%=():#,", c) != NULL) {
            token = (Token *) malloc (1 + sizeof *token);
            token->next = NULL;
            token->srcloc = q;
            token->toktype = TT_DELIM;
            token->intval = c & 0377;
            token->strval[0] = c;
            token->strval[1] = 0;
            q ++;
            *ltoken = token;
            ltoken = &token->next;
            continue;
        }

        // who knows
        return mprintf ("unknown char %c", c);
    }

    token = (Token *) malloc (sizeof *token);
    token->next = NULL;
    token->srcloc = q;
    token->toktype = TT_ENDLINE;
    token->intval = 0;
    token->strval[0] = 0;
    *ltoken = token;

    return NULL;
}

// decode instruction and operand if any
//  input:
//   token = points to opcode token
//  output:
//   returns NULL: instruction decode, *opcode_r filled in
//           else: error message
static char *decodeopcode (Token *token, uint16_t address, uint16_t *opcode_r)
{
    char const *mne;
    Symbol *symbol;
    uint16_t word;

    if (token->toktype != TT_SYMBOL) {
        return mprintf ("expecting opcode at beginning %s", token->srcloc);
    }

    mne = token->strval;

    // see if opcode symbol
    for (symbol = symbols; symbol != NULL; symbol = symbol->next) {
        if (strcasecmp (mne, symbol->name) == 0) break;
    }
    if (symbol != NULL) {
        switch (symbol->symtyp) {

            // PDP-8 memory opcode (DCA, JMP, etc)
            case ST_PMEMOP: {
                word  = symbol->value;
                token = token->next;

                // check for indirect flag ('I')
                if ((token->toktype == TT_SYMBOL) && (strcasecmp (token->strval, "I") == 0)) {
                    word |= 0400;
                    token = token->next;
                }

                // decode operand address
                if (token->toktype != TT_INTEGER) {
                    return mprintf ("expecting integer address after opcode %s", token->srcloc);
                }
                uint16_t exprvalue = token->intval;
                token = token->next;
                if (token->toktype != TT_ENDLINE) {
                    return mprintf ("expecting end of line after address %s", token->srcloc);
                }

                // see if in first 128 words of memory
                if ((exprvalue & 077600) == 0) {
                    *opcode_r = word | (exprvalue & 00177);
                    return NULL;
                }

                // see if in same page as instruction
                if (((exprvalue ^ address) & 07600) == 0) {
                    *opcode_r = word | 00200 | (exprvalue & 00177);
                    return NULL;
                }

                return mprintf ("operand address %04o out of range", exprvalue);
            }

            default: abort ();
        }
    }

    if (strcasecmp (mne, "iot") == 0) {

        // decode operand value
        token = token->next;
        if (token->toktype != TT_INTEGER) {
            return mprintf ("expecting integer i/o opcode after %s", mne);
        }
        uint16_t exprvalue = token->intval;
        if ((exprvalue < 06000) || (exprvalue > 06777)) {
            return mprintf ("expecting i/o opcode 0%04o out of range", exprvalue);
        }
        token = token->next;
        if (token->toktype != TT_ENDLINE) {
            return mprintf ("expecting end of line after i/o opcode 0%04o", exprvalue);
        }
        *opcode_r = exprvalue;
        return NULL;
    }

    // better be a micro op
    return decomicro (token, opcode_r);
}



#define O_ACS  0x0000000001ULL
#define O_ASR  0x0000000002ULL
#define O_BSW  0x0000000004ULL
#define O_CLA  0x0000000008ULL
#define O_CLL  0x0000000010ULL
#define O_CMA  0x0000000020ULL
#define O_CML  0x0000000040ULL
#define O_DAD  0x0000000080ULL
#define O_DCM  0x0000000100ULL
#define O_DPIC 0x0000000200ULL
#define O_DPSZ 0x0000000400ULL
#define O_DST  0x0000000800ULL
#define O_DVI  0x0000001000ULL
#define O_HLT  0x0000002000ULL
#define O_IAC  0x0000004000ULL
#define O_LSR  0x0000008000ULL
#define O_MQA  0x0000010000ULL
#define O_MQL  0x0000020000ULL
#define O_MUY  0x0000040000ULL
#define O_NMI  0x0000080000ULL
#define O_OSR  0x0000100000ULL
#define O_RAL  0x0000200000ULL
#define O_RAR  0x0000400000ULL
#define O_RTL  0x0000800000ULL
#define O_RTR  0x0001000000ULL
#define O_SAM  0x0002000000ULL
#define O_SCA  0x0004000000ULL
#define O_SCL  0x0008000000ULL
#define O_SHL  0x0010000000ULL
#define O_SKP  0x0020000000ULL
#define O_SMA  0x0040000000ULL
#define O_SNL  0x0080000000ULL
#define O_SWBA 0x0100000000ULL
#define O_SZA  0x0200000000ULL
#define O_SPA  0x0400000000ULL
#define O_SZL  0x0800000000ULL
#define O_SNA  0x1000000000ULL

#define GR1_SEQ1_ANY (O_CLA|O_CLL)
#define GR1_SEQ2_ANY (O_CMA|O_CML)
#define GR1_SEQ3     (O_IAC)
#define GR1_SEQ4_ONE (O_RAR|O_RAL|O_RTR|O_RTL|O_BSW)

#define GR1 (GR1_SEQ1_ANY|GR1_SEQ2_ANY|GR1_SEQ3|GR1_SEQ4_ONE)

#define GR2_SEQ1P_ANY (O_SMA|O_SZA|O_SNL)               // positive skips
#define GR2_SEQ1N_ANY (O_SPA|O_SNA|O_SZL)               // negative skips
#define GR2_SEQ1U     (O_SKP)                           // unconditional skip
#define GR2_SEQ2      (O_CLA)
#define GR2_SEQ3      (O_OSR)
#define GR2_SEQ4      (O_HLT)

#define GR2 (GR2_SEQ1P_ANY|GR2_SEQ1N_ANY|GR2_SEQ1U|GR2_SEQ2|GR2_SEQ3|GR2_SEQ4)

#define GR3A_SEQ1     (O_CLA)
#define GR3A_SEQ2_ANY (O_MQA|O_SCA|O_MQL)
#define GR3A_SEQ3_ONE (O_SCL|O_MUY|O_DVI|O_NMI|O_SHL|O_ASR|O_LSR)

#define GR3A (GR3A_SEQ1|GR3A_SEQ2_ANY|GR3A_SEQ3_ONE)

#define GR3B_SEQ1     (O_CLA)
#define GR3B_SEQ2_ANY (O_MQA|O_MQL)
#define GR3B_SEQ3_ONE (O_ACS|O_MUY|O_DVI|O_NMI|O_SHL|O_ASR|O_LSR|O_SCA|O_DAD|O_DST|O_SWBA|O_DPSZ|O_DPIC|O_DCM|O_SAM)

#define GR3B (GR3B_SEQ1|GR3B_SEQ2_ANY|GR3B_SEQ3_ONE)

typedef struct Micro {
    uint16_t opc;
    char const mne[6];
    uint64_t msk;
} Micro;

static Micro const microtable[] = {
    { 07403, "ACS",  O_ACS  },
    { 07415, "ASR",  O_ASR  },
    { 07002, "BSW",  O_BSW  },
    { 07621, "CAM",  O_CLA|O_MQL },
    { 07041, "CIA",  O_CMA|O_IAC },
    { 07200, "CLA",  O_CLA  },
    { 07100, "CLL",  O_CLL  },
    { 07040, "CMA",  O_CMA  },
    { 07020, "CML",  O_CML  },
    { 07443, "DAD",  O_DAD  },
    { 07455, "DCM",  O_DCM  },
    { 07573, "DPIC", O_DPIC },
    { 07451, "DPSZ", O_DPSZ },
    { 07445, "DST",  O_DST  },
    { 07407, "DVI",  O_DVI  },
    { 07402, "HLT",  O_HLT  },
    { 07001, "IAC",  O_IAC  },
    { 07604, "LAS",  O_CLA|O_OSR },
    { 07417, "LSR",  O_LSR  },
    { 07501, "MQA",  O_MQA  },
    { 07421, "MQL",  O_MQL  },
    { 07405, "MUY",  O_MUY  },
    { 07411, "NMI",  (uint64_t)-1LL },
    { 07000, "NOP",  (uint64_t)-1LL },
    { 07404, "OSR",  O_OSR  },
    { 07004, "RAL",  O_RAL  },
    { 07010, "RAR",  O_RAR  },
    { 07006, "RTL",  O_RTL  },
    { 07012, "RTR",  O_RTR  },
    { 07457, "SAM",  O_SAM  },
    { 07441, "SCA",  O_SCA  },
    { 07403, "SCL",  O_SCL  },
    { 07413, "SHL",  O_SHL  },
    { 07410, "SKP",  O_SKP  },
    { 07500, "SMA",  O_SMA  },
    { 07450, "SNA",  O_SNA  },
    { 07420, "SNL",  O_SNL  },
    { 07510, "SPA",  O_SPA  },
    { 07240, "STA",  O_CLA|O_CMA },
    { 07120, "STL",  O_CLL|O_CML },
    { 07431, "SWAB", (uint64_t)-1LL },  // a: mql+nmi; b: mql+nmi
    { 07447, "SWBA", (uint64_t)-1LL },  // a: sca+dvi; b: swba
    { 07440, "SZA",  O_SZA  },
    { 07430, "SZL",  O_SZL  },
    {     0, "",        0 } };

// decode micro opcodes
//  input:
//   token = points to possible micro opcode
//  output:
//   returns 0: token was not a micro opcode
//        else: decoded micro opcode 07xxx
static char *decomicro (Token *token, uint16_t *opcode_r)
{
    int i, skipnext;
    uint16_t opcode;
    uint64_t already;

    already  = 0;
    opcode   = 0;
    skipnext = 0;

    do {

        // check for opcode
        if (token->toktype != TT_SYMBOL) {
            return mprintf ("expecting micro opcode %s", token->srcloc);
        }

        // look it up in table
        Micro const *micro;
        for (i = 0;; i ++) {
            micro = &microtable[i];
            if (micro->mne[0] == 0) break;
            if (strcasecmp (token->strval, micro->mne) == 0) goto found;
        }
        return mprintf ("unknown opcode %s", token->strval);

        // check for conflict with other micro ops
        // can only conflict if second or later so always print error message
        // only merge if good to avoid excessive error messages
    found:;
        if (skipnext && ! (micro->msk & (GR2_SEQ1P_ANY | GR2_SEQ1N_ANY))) {
            return mprintf ("+ * | & allowed only between conditional skips");
        }
        char const *conf = microconflict (already, micro->msk);
        if (conf != NULL) {
            return mprintf ("conflicting or out-of-order micro opcode %s (%s)", token->strval, conf);
        } else {
            already |= micro->msk;
            opcode  |= micro->opc;
        }

        // point to next in line
        token = token->next;

        // can have + * | & between conditional skip micro ops
        skipnext = 0;
        if (token->toktype == TT_DELIM) {
            switch (token->intval) {
                case '+':
                case '|': {
                    if (micro->msk & GR2_SEQ1P_ANY) {
                        skipnext = 1;
                    } else {
                        return mprintf ("+ | only allowed between pos skips SMA SZA SNL");
                    }
                    break;
                }
                case '*':
                case '&': {
                    if (micro->msk & GR2_SEQ1N_ANY) {
                        skipnext = 1;
                    } else {
                        return mprintf ("* & only allowed between neg skips SPA SNA SZL");
                    }
                    break;
                }
                default: {
                    return mprintf ("unknown delimiter %c", token->intval);
                }
            }
            token = token->next;
        }

    } while (token->toktype != TT_ENDLINE);

    // Group 2 CLA is one cycle faster than Group 1 CLA
    // Group 2 NOP is one cycle faster than Group 1 NOP
    ////if ((opcode & 07577) == 07000) opcode |= 00400;

    *opcode_r = opcode;
    return NULL;
}

static char const *microconflict (uint64_t already, uint64_t adding)
{
    // can't have same thing twice
    if (already & adding) return "DUPLICATE";

    // if already have GR1_SEQ4_ONE (simple shift), can't add anything
    if (already & GR1_SEQ4_ONE) return "RAR RAL RTR RTL BSW";

    // if already have GR1_SEQ3 (iac), can't add GR1_SEQ2_ANY|GR1_SEQ1_ANY|GR2_*|GR3A_*|GR3B_*
    if ((already & GR1_SEQ3) && (adding & (GR1_SEQ2_ANY|GR1_SEQ1_ANY|GR2|GR3A|GR3B))) return "IAC";

    // if already have GR1_SEQ2_ANY (cma,cml), can't add GR1_SEQ1_ANY|GR2_*|GR3A_*|GR3B_*
    if ((already & GR1_SEQ2_ANY) && (adding & (GR1_SEQ1_ANY|GR2|GR3A|GR3B))) return "CMA CML";

    // if already have O_CLL, can't add GR2_*|GR3A_*|GR3B_*-O_CLA
    if ((already & O_CLL) && (adding & (GR2|GR3A|GR3B) & ~ O_CLA)) return "CLL";

    // if already have GR2_SEQ4 (hlt), can't add anything
    if (already & GR2_SEQ4) return "HLT";

    // if already have GR2_SEQ3 (osr), can't add GR1_*|GR2_SEQ1*|GR2_SEQ2|GR3A_*|GR3B_*
    if ((already & GR2_SEQ3) && (adding & (GR1|GR2_SEQ2|GR2_SEQ1U|GR2_SEQ1N_ANY|GR2_SEQ1P_ANY|GR3A|GR3B))) return "OSR";

    // if already have GR2_SEQ2 (cla), can't add GR2_SEQ1P_ANY|GR2_SEQ1N_ANY|GR2_SEQ1U
    if ((already & GR2_SEQ2) && (adding & (GR2_SEQ1U|GR2_SEQ1N_ANY|GR2_SEQ1P_ANY))) return "CLA";

    // if already have GR2_SEQ1U (skp), can't add GR1_*|GR2_SEQ1N_ANY|GR2_SEQ1P_ANY|GR3A_*|GR3B_*-O_CLA
    if ((already & GR2_SEQ1U) && (adding & (GR1|GR2_SEQ1N_ANY|GR2_SEQ1P_ANY|GR3A|GR3B) & ~ O_CLA)) return "SKP";

    // if already have GR2_SEQ1N_ANY (spa,sna,szl), can't add GR1_*|GR2_SEQ1P_ANY|GR2_SEQ1U|GR3A_*|GR3B_*-O_CLA
    if ((already & GR2_SEQ1N_ANY) && (adding & (GR1|GR2_SEQ1U|GR2_SEQ1P_ANY|GR3A|GR3B) & ~ O_CLA)) return "SPA SNA SZL";

    // if already have GR2_SEQ1P_ANY (sma,sza,snl), can't add GR1_*|GR2_SEQ1N_ANY|GR2_SEQ1U|GR3A_*|GR3B_*-O_CLA
    if ((already & GR2_SEQ1P_ANY) && (adding & (GR1|GR2_SEQ1U|GR2_SEQ1N_ANY|GR3A|GR3B) & ~ O_CLA)) return "SMA SZA SNL";

    // if already have GR3A_SEQ3_ONE (ops), can't add anything
    if (already & GR3A_SEQ3_ONE) return "SCL MUY DVI NMI SHL ASR LSR";

    // if already have GR3A_SEQ2_ANY (mqa,sca,mql), can't add GR1_*|GR2_*|GR3A_SEQ1|GR3B_*
    if ((already & GR3A_SEQ2_ANY) && (adding & (GR1|GR2|GR3A_SEQ1|GR3B))) return "MQA SCA MQL";

    // if already have GR3B_SEQ3_ONE (ops), can't add anything
    if (already & GR3A_SEQ3_ONE) return "SCL MUY DVI NMI SHL ASR LSR";

    // if already have GR3B_SEQ2_ANY (mqa,mql), can't add GR1_*|GR2_*|GR3B_SEQ1|GR3A_*
    if ((already & GR3B_SEQ2_ANY) && (adding & (GR1|GR2|GR3B_SEQ1|GR3A))) return "SCL MUY DVI NMI SHL ASR LSR";

    // no conflict
    return NULL;
}

// malloc buffer to print into
static char *mprintf (char const *fmt, ...)
{
    char *str = NULL;
    va_list ap;
    va_start (ap, fmt);
    if (vasprintf (&str, fmt, ap) < 0) abort ();
    if (str == NULL) abort ();
    va_end (ap);
    return str;
}
