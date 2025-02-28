/*
** cpu.h unSP cpu-description header-file
** (c) in 2021-2024 by Adrien Destugues
*/

#define BIGENDIAN 0
#define LITTLEENDIAN 1
#define BITSPERBYTE 16
#define VASM_CPU_UNSP 1

/* maximum number of operands for one mnemonic */
#define MAX_OPERANDS 3

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* valid parentheses for cpu's operands */
#define START_PARENTH(x) ((x)=='(' || (x)=='[')
#define END_PARENTH(x) ((x)==')' || (x)==']')

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

/* minimum instruction alignment */
#define INST_ALIGN 1

/* default alignment for n-bit data */
#define DATA_ALIGN(n) 1

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) MIMM

/* returns true when instruction is valid for selected cpu */
#define MNEMONIC_VALID(i) cpu_available(i)

/* we define two additional unary operations, '<' and '>' */
int ext_unary_eval(int,taddr,taddr *,int);
int ext_find_base(symbol **,expr *,section *,taddr);
#define LOBYTE (LAST_EXP_TYPE+1)
#define HIBYTE (LAST_EXP_TYPE+2)
#define EXT_UNARY_NAME(s) (*s=='<'||*s=='>')
#define EXT_UNARY_TYPE(s) (*s=='<'?LOBYTE:HIBYTE)
#define EXT_UNARY_EVAL(t,v,r,c) ext_unary_eval(t,v,r,c)
#define EXT_FIND_BASE(b,e,s,p) ext_find_base(b,e,s,p)

/* operand types */
enum {
  _NO_OP = 0, /* no operand */
  MIMM6   = 1, /* 6-bit immediate */
  MIMM16  = 2, /* 16-bit immediate */
  MIMM    = 3, /* 6 or 16-bit immediate */
  MBP6    = 4, /* BP-relative */
  MA6     = 8, /* 6 bit address */
  MA16    = 16, /* 16 bit address */
  MADDR   = MA6 | MA16, /* 6 or 16-bit address */
  MREG    = 32, /* register */
  MREGA   = 64, /* address register */
  MANY    = 127, /* any of the above */
  MMEMORY = MBP6 | MADDR | MREGA, /* Somewhere in memory */
  MA22    = 128, /* 22 bit address */
  MEXT    = MA22 | MA16 | MIMM16, /* needs a second 16 bit word */
  MPC6    = 256, /* 6-bit PC relative */
  MVALUE  = MADDR | MIMM | MBP6 | MPC6,
  M6BIT   = MIMM6 | MA6 | MBP6 | MPC6,
  M16BIT  = MIMM16 | MA16,
  MONOFF  = 512, /* on/off flag */
  MIRQ    = 1024 /* irq flag */
};

enum {
	_NO_FLAGS = 0,
	FPOSTDEC,
	FPOSTINC,
	FPREINC,
	FDSEG
};

/* type to store each operand */
typedef struct {
  uint16_t mode;
  uint16_t flags;
  int8_t reg;
  expr *value;
} operand;

/* evaluated expressions */
struct MyOpVal {
  taddr value;
  symbol *base;
  int btype;
};

/* additional mnemonic data */
typedef struct {
  uint16_t opcode;
  uint8_t format;
  uint8_t flags;
} mnemonic_extension;

/* instruction format */
enum {
  NO,        /* no operand */
  ALU,       /* ALU instructions */
  STORE,     /* ST instruction */
  ALUSTORE,  /* 3-op ALU instructions with destination address */
  PUSH,      /* PUSH */
  POP,       /* POP */
  MUL1,      /* MUL */
  MAC,       /* MAC */
  SO,        /* oooooooooommmrrr: single operand */
  XJMP,      /* extended conditional jump pseudo-instruction */
  FIRMOV,    /* FIR_MOV */
  FIQ        /* FIQ */
};

/* register symbols */
#define HAVE_REGSYMS
#define REGSYMHTSIZE 64
#define RTYPE_R  0       /* Register R0..R7 */

/* exported by cpu.c */
int cpu_available(int);
/*int parse_cpu_label(char *,char **);*/
/*
** cpu.h HANS cpu-description header-file
** (c) in 2023 by Frank Wille and Yannik Stamm
*/

#define BIGENDIAN 1
#define LITTLEENDIAN 0
#define BITSPERBYTE 32
#define VASM_CPU_HANS 1

/* maximum number of operands for one mnemonic */
#define MAX_OPERANDS 3

/* make sure operand is cleared upon first entry into parse_operand() */
#define CLEAR_OPERANDS_ON_START 1

/* maximum number of mnemonic-qualifiers per mnemonic */
#define MAX_QUALIFIERS 0

/* data type to represent a target-address */
typedef int32_t taddr;
typedef uint32_t utaddr;

/* minimum instruction alignment */
#define INST_ALIGN 1

/* default alignment for n-bit data */
#define DATA_ALIGN(n) 1

/* operand class for n-bit data definitions */
#define DATA_OPERAND(n) Data

/* returns true when instruction is valid for selected cpu */
#define MNEMONIC_VALID(i) 1

/* parse cpu-specific directives with label */
/*#define PARSE_CPU_LABEL(l,s) parse_cpu_label(l,s)*/


/* operand types */
enum {
  None=0,                   /* none */
  Data,                     /* n-bit data */
  SourceReg1,               /* general purpose register 1 */
  SourceReg2,               /* general purpose register 2 */
  TargetReg,                /* target general purpose register */
  SourceFloatReg1,          /* floating point register 1 */
  SourceFloatReg2,          /* floating point register 2 */
  TargetFloatReg,           /* target floating point register */
  Immediate16,              /* 16-bit signed immediate for I-format */
  Immediate16Plus1,         /* 16-bit signed immediate for I-format. Immediate increased by 1 */
  Immediate16Minus1,        /* 16-bit signed immediate for I-format. Immediate decreased by 1 */
  Immediate16Label,         /* 16-bit PC-relative label for I-format */
  Immediate26Label          /* 26-bit PC-relative label for J-format */
};

enum
{
    DefaultLabel,
    LowLabel,
    HighLabel,
    HighAlgebraicLabel
};

/* type to store each operand */
typedef struct {
  int reg;
  expr *exp;
  int labelType;
} operand;

/* additional mnemonic data */
typedef struct {
  uint32_t opcode;
} mnemonic_extension;

/* instruction formats */
#define FORMR(x) ((x)&0x3f)
#define FORMI(x) ((0x20|((x)&0x1f))<<26)
#define FORMJ(x) ((0x10|((x)&0xf))<<26)

/* register symbols */
#define HAVE_REGSYMS
#define REGSYMHTSIZE 64
#define RTYPE_R  0       /* Register R0..R31 */
#define RTYPE_F  1       /* Register F0..F31 */

/* exported by cpu.c */
/*int cpu_available(int);*/
/*int parse_cpu_label(char *,char **);*/
