/*
 *  FX-850 keywords (Should work for PB-1000 as well)
 *  Double byte tokens from 0x40 to 0xCF with prefixes 4 to 7
 */
char *tokens850[ 4 ][ 0xd0 - 0x40 ] = 
{ 
    { /* Prefix 4 */
        NULL,       NULL,       NULL,       NULL,       /* 40 */  
        NULL,       NULL,       NULL,       NULL,       /* 44 */  
        NULL,       "GOTO ",    "GOSUB ",   "RETURN",   /* 48 */  
        "RESUME ",  "RESTORE ", "WRITE#",   NULL,       /* 4C */  
        NULL,       NULL,       "SYSTEM",   "PASS ",    /* 50 */  
        NULL,       "DELETE ",  "BSAVE ",   "LIST ",    /* 54 */  
        "LLIST ",   "LOAD ",    "MERGE ",   NULL,       /* 58 */  
        NULL,       "TRON",     NULL,       "TROFF",    /* 5C */  
        "VERIFY ",  "MON",      "CALL ",    "POKE ",    /* 60 */  
        NULL,       NULL,       NULL,       NULL,       /* 64 */  
        NULL,       "CHAIN ",   "CLEAR ",   "NEW ",     /* 68 */  
        "SAVE ",    "RUN ",     "ANGLE ",   "EDIT ",    /* 6C */  
        "BEEP ",    "CLS",      "CLOSE ",   NULL,       /* 70 */  
        NULL,       NULL,       "DEF ",     "DEFM ",    /* 74 */  
        "DEFSEG ",  NULL,       "VAC",      NULL,       /* 78 */  
        "DIM ",     "DRAW ",    NULL,       NULL,       /* 7C */  
        "DATA ",    "FOR ",     "NEXT ",    NULL,       /* 80 */  
        NULL,       "ERASE ",   "ERROR ",   "END",      /* 84 */  
        NULL,       NULL,       "FIELD ",   "FORMAT",   /* 88 */  
        "GET ",     "IF ",      NULL,       "LET ",     /* 8C */  
        "LINE ",    "LOCATE ",  NULL,       "LSET ",    /* 90 */  
        NULL,       NULL,       NULL,       "OPEN ",    /* 94 */  
        NULL,       "OUT ",     "ON ",      NULL,       /* 98 */  
        NULL,       NULL,       NULL,       "CALCJMP ", /* 9C */  
        "BLOAD ",   NULL,       "DRAWC ",   "PRINT ",   /* A0 */  
        "LPRINT ",  "PUT ",     NULL,       NULL,       /* A4 */  
        "READ ",    "REM ",     "RSET ",    NULL,       /* A8 */  
        "SET ",     "STAT",     "STOP",     NULL,       /* AC */  
        "MODE ",    NULL,       "VAR ",     "PBLOAD ",  /* B0 */  
        "PBGET ",   NULL,       NULL,       NULL,       /* B4 */  
        NULL,       NULL,       NULL,       NULL,       /* B8 */  
        NULL,       NULL,       NULL,       NULL,       /* BC */  
        NULL,       NULL,       NULL,       NULL,       /* C0 */  
        NULL,       NULL,       NULL,       NULL,       /* C4 */  
        NULL,       NULL,       NULL,       NULL,       /* C8 */  
        NULL,       NULL,       NULL,       NULL        /* CC */  
    },
    { /* prefix 5 */
        NULL,       NULL,       NULL,       NULL,       /* 40 */
        NULL,       NULL,       NULL,       NULL,       /* 44 */
        NULL,       NULL,       NULL,       NULL,       /* 48 */
        NULL,       NULL,       NULL,       "ERL",      /* 4C */
        "ERR",      "CNT",      "SUMX",     "SUMY",     /* 50 */
        "SUMX2",    "SUMY2",    "SUMXY",    "MEANX",    /* 54 */
        "MEANY",    "SDX",      "SDY",      "SDXN",     /* 58 */
        "SDYN",     "LRA",      "LRB",      "COR",      /* 5C */
        "PI",       NULL,       NULL,       "CUR ",     /* 60 */
        NULL,       NULL,       NULL,       "FACT ",    /* 64 */
        NULL,       "EOX ",     "EOY ",     "SIN ",     /* 68 */
        "COS ",     "TAN ",     "ASN ",     "ACS ",     /* 6C */
        "ATN ",     "HYPSIN ",  "HYPCOS ",  "HYPTAN ",  /* 70 */
        "HYPASN ",  "HYPACS ",  "HYPATN ",  "LN ",      /* 74 */
        "LOG ",     "EXP ",     "SQR ",     "ABS ",     /* 78 */
        "SGN ",     "INT ",     "FIX ",     "FRAC ",    /* 7C */
        "RND ",     NULL,       NULL,       NULL,       /* 80 */
        NULL,       NULL,       "PEEK ",    NULL,       /* 84 */
        NULL,       "LOF ",     "EOF ",     NULL,       /* 88 */
        NULL,       "FRE ",     NULL,       "POINT ",   /* 8C */
        "ROUND",    "RND",      "VALF",     "RAN#",     /* 90 */
        "ASC",      "LEN",      "VAL",      NULL,       /* 94 */
        NULL,       NULL,       NULL,       NULL,       /* 98 */
        "DEG",      NULL,       NULL,       NULL,       /* 9C */
        NULL,       NULL,       NULL,       NULL,       /* A0 */
        NULL,       NULL,       NULL,       "REC",      /* A4 */
        "POL",      NULL,       "NPR",      "NCR",      /* A8 */
        "HYP",      NULL,       NULL,       NULL,       /* AC */
        NULL,       NULL,       NULL,       NULL,       /* B0 */
        NULL,       NULL,       NULL,       NULL,       /* B4 */
        NULL,       NULL,       NULL,       NULL,       /* B8 */
        NULL,       NULL,       NULL,       NULL,       /* BC */
        NULL,       NULL,       NULL,       NULL,       /* C0 */
        NULL,       NULL,       NULL,       NULL,       /* C4 */
        NULL,       NULL,       NULL,       NULL,       /* C8 */
        NULL,       NULL,       NULL,       NULL        /* CC */
    },
    { /* prefix 6 */
        NULL,       NULL,       NULL,       NULL,       /* 40 */
        NULL,       NULL,       NULL,       NULL,       /* 44 */
        NULL,       NULL,       NULL,       NULL,       /* 48 */
        NULL,       NULL,       NULL,       NULL,       /* 4C */
        NULL,       NULL,       NULL,       NULL,       /* 50 */
        NULL,       NULL,       NULL,       NULL,       /* 54 */
        NULL,       NULL,       NULL,       NULL,       /* 58 */
        NULL,       NULL,       NULL,       NULL,       /* 5C */
        NULL,       NULL,       NULL,       NULL,       /* 60 */
        NULL,       NULL,       NULL,       NULL,       /* 64 */
        NULL,       NULL,       NULL,       NULL,       /* 68 */
        NULL,       NULL,       NULL,       NULL,       /* 6C */
        NULL,       NULL,       NULL,       NULL,       /* 70 */
        NULL,       NULL,       NULL,       NULL,       /* 74 */
        NULL,       NULL,       NULL,       NULL,       /* 78 */
        NULL,       NULL,       NULL,       NULL,       /* 7C */
        NULL,       NULL,       NULL,       NULL,       /* 80 */
        NULL,       NULL,       NULL,       NULL,       /* 84 */
        NULL,       NULL,       NULL,       NULL,       /* 88 */
        NULL,       NULL,       NULL,       NULL,       /* 8C */
        NULL,       NULL,       NULL,       NULL,       /* 90 */
        NULL,       NULL,       NULL,       "DMS$",     /* 94 */
        NULL,       NULL,       "MID",      "INPUT ",   /* 98 */
        "MID$",     "RIGHT$",   "LEFT$",    NULL,       /* 9C */
        "CHR$",     "STR$",     NULL,       "HEX$",     /* A0 */
        NULL,       NULL,       NULL,       NULL,       /* A4 */
        "INKEY$",   "KEY",      NULL,       "DATE$",    /* A8 */
        "TIME$",    "CALC$",    NULL,       NULL,       /* AC */
        NULL,       NULL,       NULL,       NULL,       /* B0 */
        NULL,       NULL,       NULL,       NULL,       /* B4 */
        NULL,       NULL,       NULL,       NULL,       /* B8 */
        NULL,       NULL,       NULL,       NULL,       /* BC */
        NULL,       NULL,       NULL,       NULL,       /* C0 */
        NULL,       NULL,       NULL,       NULL,       /* C4 */
        NULL,       NULL,       NULL,       NULL,       /* C8 */
        NULL,       NULL,       NULL,       NULL        /* CC */
    },
    { /* prefix 7 */
        NULL,       NULL,       NULL,       NULL,       /* 40 */
        NULL,       NULL,       NULL,       " THEN ",   /* 44 */
        "ELSE ",    NULL,       NULL,       NULL,       /* 48 */
        NULL,       NULL,       NULL,       NULL,       /* 4C */
        NULL,       NULL,       NULL,       NULL,       /* 50 */
        NULL,       NULL,       NULL,       NULL,       /* 54 */
        NULL,       NULL,       NULL,       NULL,       /* 58 */
        NULL,       NULL,       NULL,       NULL,       /* 5C */
        NULL,       NULL,       NULL,       NULL,       /* 60 */
        NULL,       NULL,       NULL,       NULL,       /* 64 */
        NULL,       NULL,       NULL,       NULL,       /* 68 */
        NULL,       NULL,       NULL,       NULL,       /* 6C */
        NULL,       NULL,       NULL,       NULL,       /* 70 */
        NULL,       NULL,       NULL,       NULL,       /* 74 */
        NULL,       NULL,       NULL,       NULL,       /* 78 */
        NULL,       NULL,       NULL,       NULL,       /* 7C */
        NULL,       NULL,       NULL,       NULL,       /* 80 */
        NULL,       NULL,       NULL,       NULL,       /* 84 */
        NULL,       NULL,       NULL,       NULL,       /* 88 */
        NULL,       NULL,       NULL,       NULL,       /* 8C */
        NULL,       NULL,       NULL,       NULL,       /* 90 */
        NULL,       NULL,       NULL,       NULL,       /* 94 */
        NULL,       NULL,       NULL,       NULL,       /* 98 */
        NULL,       NULL,       NULL,       NULL,       /* 9C */
        NULL,       NULL,       NULL,       NULL,       /* A0 */
        NULL,       NULL,       NULL,       NULL,       /* A4 */
        NULL,       NULL,       NULL,       NULL,       /* A8 */
        NULL,       NULL,       NULL,       NULL,       /* AC */
        NULL,       NULL,       NULL,       NULL,       /* B0 */
        NULL,       NULL,       "TAB ",     NULL,       /* B4 */
        "CSR ",     "REV ",     "NORM ",    "ALL ",     /* B8 */
        " AS ",     "APPEND ",  NULL,       "OFF",      /* BC */
        " STEP ",   " TO ",     "USING ",   "NOT ",     /* C0 */
        " AND ",    " OR ",     " XOR ",    " MOD ",    /* C4 */
        NULL,       NULL,       NULL,       NULL,       /* C8 */
        NULL,       NULL,       NULL,       NULL        /* CC */
    }
};
