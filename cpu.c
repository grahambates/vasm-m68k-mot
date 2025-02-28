/*
 * cpu.c unSP cpu description file
 */

#include "vasm.h"

mnemonic mnemonics[] = {
#include "opcodes.h"
};
const int mnemonic_cnt = sizeof(mnemonics) / sizeof(mnemonics[0]);

const char *cpu_copyright = "vasm unSP cpu backend 0.4 (c) 2021-2024 by Adrien Destugues";
const char *cpuname = "unsp";

int bytespertaddr = 2;

/* Variables set by special parsing */
static int modifier;  /* set by find_base() */

operand *new_operand(void)
{
  operand *new = mymalloc(sizeof(*new));
  return new;
}

static int parse_reg(char **start)
{
  char *p = *start;
  regsym *sym;

  if (ISIDSTART(*p)) {
    p++;
    while (ISIDCHAR(*p))
      p++;
    if (sym = find_regsym_nc(*start,p-*start)) {
      *start = p;
      return sym->reg_num;
    }
  }

  return -1;
}

int ext_unary_eval(int type,taddr val,taddr *result,int cnst)
{
    switch (type) {
    case LOBYTE:
        *result = cnst ? (val & 0xffff) : val;
        return 1;
    case HIBYTE:
        *result = cnst ? ((val >> 16) & 0xffff) : val;
        return 1;
    default:
        break;
    }
    return 0;  /* unknown type */
}

int ext_find_base(symbol **base,expr *p,section *sec,taddr pc)
{
    if ( p->type==DIV || p->type ==MOD) {
        if (p->right->type == NUM && p->right->c.val == 65536 ) {
           p->type = p->type == DIV ? HIBYTE : LOBYTE;
        }
    } else if ( p->type == BAND && p->right->type == NUM && p->right->c.val == 65535 ) {
        p->type = LOBYTE;
    }
    if (p->type==LOBYTE || p->type==HIBYTE) {
        modifier = p->type;
        return find_base(p->left,base,sec,pc);
    }
    return BASE_ILLEGAL;
}

static taddr apply_modifier(rlist *rl, taddr val)
{
    switch (modifier) {
    case LOBYTE:
        if (rl)
            ((nreloc *)rl->reloc)->mask = 0xffff;
        val = val & 0xffff;
        break;
    case HIBYTE:
        if (rl)
            ((nreloc *)rl->reloc)->mask = 0x3f0000;
        val = (val >> 16) & 0xffff;
        break;
    }
    return val;
}

int parse_operand(char *p,int len,operand *op,int required)
{
  char   *start = p;

  op->value = NULL;
  op->flags = 0;
  p = skip(p);

  if (required & MREG) {
      if ((op->reg = parse_reg(&p)) >= 0) {
        /* register direct */
        op->mode = MREG;
        p = skip(p);

        if (p >= start + len)
            return PO_MATCH;

        if (strnicmp("asr", p, 3) == 0)
            op->flags = 044;
        else if (strnicmp("lsl", p, 3) == 0)
            op->flags = 050;
        else if (strnicmp("lsr", p, 3) == 0)
            op->flags = 054;
        else if (strnicmp("rol", p, 3) == 0)
            op->flags = 060;
        else if (strnicmp("ror", p, 3) == 0)
            op->flags = 064;
        else
            cpu_error(2); /* unrecognized shift operand */

        if (op->flags != 0) {
            /* parse shift amount into op->value */
            p +=2;
            p = skip(p + 1);

            op->value = parse_expr(&p);
        }
        return PO_MATCH;
      }
  }

  if (required & (MREGA | MADDR)) {
      if (*p == 'D' && *(p+1) == 'S' && *(p+2) == ':') {
          /* DS-based */
          op->flags = FDSEG;
          p = skip(p+3);
      }


      if (*p == '(' && check_indir(p, start+len)) {
        int   llen;
        char *end;
        p++;

        p = skip(p);

        if (*p == '+' && *(p+1) == '+') {
            /* Pre-increment */
            op->flags |= FPREINC;
            p += 2;
        }
        /* TODO Pre-decrement */

        /* Search for the first nonmatching character */
        if ( (end = strpbrk(p, ") +-") ) != NULL ) {
            llen = end - p;
        } else {
            llen = len - (p - start);
        }

        if ((op->reg = parse_reg(&p)) >= 0) {
            /* It's a register */
            op->mode = MREGA;
        } else {
            /* Maybe it's an address? */
            op->value = parse_expr(&p);
            op->mode = MADDR;
            /* No pre/post increment allowed in that case so we return directly */
            return PO_MATCH;
        }

        if (*end == '-' && *(end+1) == '-') {
            /* Post-decrement */
            op->flags |= FPOSTDEC;
            p = end + 2;
        } else if (*end == '+' && *(end+1) == '+') {
            /* Post-increment */
            op->flags |= FPOSTINC;
            p = end + 2;
        } else if (op->reg == 5 && *end == '+') {
            /* BP+disp */
            op->value = parse_expr(&p);
            op->mode = MBP6;
            p = end + 1;
        }

        return PO_MATCH;
      }
  }

  if (required & (MA22 | MPC6 | MIMM)) {
      /* Jumps, immediate values */
      if (*p == '#') {
        p = skip(p+1);
      }
      op->value = parse_expr(&p);
      op->mode = required & (MA22 | MPC6 | MIMM);
      return PO_MATCH;
  }

  if (required & MONOFF) {
      if (strnicmp(p, "on", 2) == 0) {
        op->value = number_expr(1);
        op->mode = MONOFF;
      }
      else if (strnicmp(p, "irq", 3) == 0) {
        op->value = number_expr(1);
        op->mode = MONOFF;
      }
      else if (strnicmp(p, "fiq", 3) == 0) {
        op->value = number_expr(2);
        op->mode = MONOFF;
      }
      else if (strnicmp(p, "off", 3) == 0) {
        op->value = number_expr(0);
        op->mode = MONOFF;
      } else {
        return PO_NOMATCH;
      }
      return PO_MATCH;
  }

  return PO_NOMATCH;
}

char *parse_cpu_special(char *s)
{
  return s;  /* nothing special */
}

static uint16_t encode_opa(operand* op)
{
    return op->reg << 9;
}

static uint16_t encode_opb(operand* op, dblock* db, struct MyOpVal* value, section* sec, taddr pc)
{
    if (op->mode & (MVALUE | MONOFF)) {

        symbol *base = NULL;
        int is_const = eval_expr(op->value, &value->value, sec, pc);
        if (!is_const && db != NULL) {
            modifier = 0;
            if ( find_base(op->value, &base, sec, pc) != BASE_OK) {
                general_error(38); /* illegal relocation */
            }
        }

        if (op->mode == MPC6) {
            /* adjust for PC pre-incrementation (it is already pointing to the next instruction) */
            value->value -= pc + 1;
            /* decide if a relocation is necessary
             * check is_pc_reloc (from reloc.c), if the jump is PC relative, it's likely that
             * we already know the ofset at assembling time because the target is in the
             * same section, and there's no need to relocate it at link time.
             */
            if (base != NULL && is_pc_reloc(base, sec)) {
                add_extnreloc(&db->relocs,base, value->value, REL_PC, 10, 6, 0);
            }
            /* During the first pass, db will be NULL, so we can't get the correct value.
             * During the second pass, db will be set and value is correct.
             *
             * Returning an invalid opcode in the first pass doesn't matter as long as we have the
             * correct size. */
            if (db != NULL) {
                if ((value->value > 63) || (value->value < -63))
                    cpu_error(3, value->value); /* jump target out of range */
            }
        } else {
            if ((value->value & ~0xFFFF) != 0) {
                /* Out of range for 16 bit unsigned */
                op->mode &= ~M16BIT;
            }

            if (!is_const) {
                /* 6-bit mode not allowed for external relocations, because we can't know if the
                 * final value will fit. So always use a 16-bit relocation here.
                 * TODO also allow 22-bit relocations if needed? */
                op->mode &= ~M6BIT;
                if (op->mode & M16BIT) {
                    if (base != NULL) {
                        rlist* rl = add_extnreloc(&db->relocs,base, value->value, REL_ABS, 0, 16, 1);
                        value->value = apply_modifier(rl, value->value);
                    }
                } else
                    cpu_error(5); /* out of range */
            } else {
                if ((value->value & ~077) != 0) {
                    /* Out of range for 6 bit (unsigned) offset */
                    op->mode &= ~M6BIT;
                }

                if ((op->mode & M6BIT) && (op->mode & M16BIT)) {
                    /* The value can fit in both 6 and 16-bit, use the shortest encoding */
                    op->mode &= ~M16BIT;
                }

                if (op->mode == 0) {
                    cpu_error(5); /* out of range */
                }
            }
        }
    }

    if (op->mode == MA22) {
        if (!eval_expr(op->value, &value->value, sec, pc) && db != NULL) {
            symbol *base;
            /* FIXME can these be made optional? Are there cases where the address is known but
             * eval_expr can't tell? */
            modifier = 0;
            if ( find_base(op->value, &base, sec, pc) == BASE_OK) {
#if BIGENDIAN
                add_extnreloc(&db->relocs,base, value->value, REL_ABS, 10, 22, 0);
#else
                add_extnreloc_masked(&db->relocs,base, value->value, REL_ABS, 10, 6, 0, 0x3F0000);
                add_extnreloc_masked(&db->relocs,base, value->value, REL_ABS, 0, 16, 1, 0xFFFF);
#endif
            } else
                general_error(38); /* illegal relocation (we don't allow any relocations for now) */
        }

        if (value->value & ~((1 << 22) - 1))
            general_error(38); /* out of range */
    }

    if (op->mode & MREG) {
        if (op->flags != 0) {
            if (!eval_expr(op->value, &value->value, sec, pc))
                general_error(38); /* illegal relocation (we don't allow any relocations for now) */
            if (value->value < 1 || value->value > 4)
                general_error(38); /* out of range */
            /* shift values are encoded as 0-3 to fit on 2 bits */
            value->value -= 1;
        } else {
            value->value = 0;
        }
    }

    /* At this point there should be a single mode left in the operand */
    switch (op->mode) {
        case MREG:
            return 0400 | (op->flags << 3) | (value->value << 3) | op->reg << 0;
        case MREGA:
            return 0300 | (op->flags << 3) | op->reg << 0;
        case MBP6:
            return (0 << 6) | (value->value << 0);
        case MIMM6:
            return (1 << 6) | (value->value << 0);
        case MIMM16:
            return 0410;
        case MA6:
            return (7 << 6) | (value->value << 0);
        case MA16:
            return 0420;
        case MA22:
            return value->value >> 16;
        case MPC6:
            if (value->value > 0)
                return value->value;
            else
                return (1 << 6) | (-value->value & 0x3f);
        case MONOFF:
            return value->value & 3;
        default: {
            ierror(0);
            return 0;
        }
    }
}

static uint16_t encode_opc(operand* op, dblock* db, section* sec, taddr pc)
{
    struct MyOpVal value;
    if (!eval_expr(op->value, &value.value, sec, pc) && db != NULL) {
        symbol *base;
        /* FIXME can these be made optional? Are there cases where the address is known but
         * eval_expr can't tell? */
        modifier = 0;
        if ( find_base(op->value, &base, sec, pc) == BASE_OK) {
            rlist* rl = add_extnreloc(&db->relocs,base, value.value, REL_ABS, 0, 16, 1);
            value.value = apply_modifier(rl, value.value);
        } else {
           general_error(38); /* illegal relocation */
        }
    }

    return value.value;
}

static size_t process_instruction(instruction* ip, section* sec, taddr pc, uint16_t* oc, uint32_t* extra, dblock* db)
{
  mnemonic *mnemo = &mnemonics[ip->code];
  size_t size = 1;
  struct MyOpVal value;

  *oc = mnemo->ext.opcode;
  *extra = 0;
  switch (mnemo->ext.format) {
    case ALU:
       if (ip->op[0]->reg == 7) {
            /* destination register is PC, disable #Imm6 addressing modes because its  encoding
             * conflicts with jump instructions (#Imm16 will be used instead). */
           ip->op[1]->mode &= ~MIMM6;
       }
       *oc |= encode_opa(ip->op[0]) | encode_opb(ip->op[1], db, &value, sec, pc);
       if (ip->op[2] != NULL) {
           /* 3-operand ALU instruction, second operand must be a register */
           if (ip->op[1]->mode != MREG)
               cpu_error(6); /* addressing mode not supported */
           if (ip->op[2]->mode & MA16)
               *oc |= 2 << 3;
           else if (ip->op[2]->mode & MIMM16)
               *oc |= 1 << 3;
           size = 2;
           *extra = encode_opc(ip->op[2], db, sec, pc);
       } else if (ip->op[0]->mode & MEXT || ip->op[1]->mode & MEXT) {
           size = 2;
           *extra = value.value;

           if (ip->op[1]->mode & M16BIT) {
                /* These modes do not use the "operand B" bits, other assemblers seem to copy the
                 * "operand A" register value there, so, just do the same */
                *oc |= (*oc & 07000) >> 9;
           }
        } else if ((ip->op[1]->mode == MBP6) && (ip->op[0]->reg == 7)) {
            /* Not allowed: would conflict with jump instructions */
            cpu_error(8);
        }
       break;
    case ALUSTORE:
       /* 3 operand instructions using the "To [Addr16]" addressing mode. op[0] will be the 16-bit
        * address and the two other operands are always registers without special addressing.
        * op[2] is not used for example in the NEG [Addr16],Rx instruction (special case for NEG
        * and CMP, which have no 3-operand form).
        */
        *oc |= encode_opa(ip->op[1]);
        if (ip->op[2] != NULL) {
            *oc |= encode_opa(ip->op[2]) >> 9;
        } else {
            /* FIXME do we need to copy op[1] in the field here? */
        }
       /* Addressing mode can only be "To [Addr16]" */
       *oc |= 0000430;
       size = 2;
       *extra = encode_opc(ip->op[0], db, sec, pc);
       break;
    case STORE:
       /* Actually the same as ALU instructions, but operands are reversed */
       *oc |= encode_opa(ip->op[0]) | encode_opb(ip->op[1], db, &value, sec, pc);
       if (ip->op[1]->mode == MA16) {
           /* Special case: for store, the Addr16 mode is encoded as 3 instead of 2 */
           *oc |= 0000030;
       }
       if (ip->op[0]->mode & MEXT || ip->op[1]->mode & MEXT) {
           size = 2;
           *extra = value.value;

           if (ip->op[1]->mode & M16BIT) {
                /* These modes do not use the "operand B" bits, other assemblers seem to copy the
                 * "operand A" register value there, so, just do the same */
                *oc |= (*oc & 07000) >> 9;
           }
        }
       break;
    case PUSH: {
       /* Op A: first reg to push (highest reg) */
       int firstReg = ip->op[1]->reg;
       /* Op N: number of regs to push */
       int regCount = ip->op[1]->reg - ip->op[0]->reg + 1;
       /* Op B: stack pointer (SP is the default) */
       int stackReg = 0;
       if (ip->op[2] != NULL)
           stackReg = ip->op[2]->reg;
       if (firstReg < 0 || regCount < 0) {
           /* TODO reorder the registers if operands 0 and 1 are swapped */
           cpu_error(0);
       }
       *oc |= (firstReg << 9) | (regCount << 3) | stackReg;
       break;
    }
    case POP: {
       /* Op A: first reg to pop (lowest reg) - 1 */
       int firstReg = ip->op[0]->reg - 1;
       /* Op N: number of regs to push */
       int regCount = ip->op[1]->reg - ip->op[0]->reg + 1;
       /* Op B: stack pointer (SP is the default) */
       int stackReg = 0;
       if (ip->op[2] != NULL)
           stackReg = ip->op[2]->reg;
       if (firstReg < 0 || regCount < 0) {
           /* TODO reorder the registers if operands 0 and 1 are swapped */
           cpu_error(0);
       }
       *oc |= (firstReg << 9) | (regCount << 3) | stackReg;
       break;
    }

    case MAC: {
        eval_expr(ip->op[2]->value, &value.value, sec, pc);
        if (value.value > 16)
            cpu_error(1); /* invalid repeat count */
        /* The official assembler uses N=0 to MAC 16 values. We accept either 0 or 16. */
        if (value.value == 16)
            value.value = 0;
        *oc |= value.value << 3;
        *oc |= ip->op[0]->reg << 9 | ip->op[1]->reg;
        break;
    }
    case MUL1: {
        *oc |= (ip->op[0]->reg << 9) | ip->op[1]->reg;
        break;
    }

    case SO: {
        /* Single operand (jump instructions) */
        *oc |= encode_opb(ip->op[0], db, &value, sec, pc);
        if (ip->op[0]->mode == MA22 || ip->op[0]->mode == MIMM16) {
            size = 2;
            *extra = value.value;
        }

        /* INT instruction optionally has a second operand */
        if (ip->op[1] && ip->op[1]->mode == MONOFF) {
            *oc |= encode_opb(ip->op[1], db, &value, sec, pc);
        }
        break;
    }

    case XJMP: {
        /* Extended conditional jump instructions. Made of a reverse conditional jump to PC+2 and
         * a GOTO instruction, if needed to reach outside the range of a 6 bit relative jump.
         */

        symbol *base = NULL;
        int32_t pcdisp;

        if (!eval_expr(ip->op[0]->value, &value.value, sec, pc) && db != NULL) {
            modifier = 0;
            if ( find_base(ip->op[0]->value, &base, sec, pc) != BASE_OK) {
                general_error(38); /* illegal relocation */
            }
        }

        pcdisp = value.value - pc - 1;
        if ((pcdisp > 63) || (pcdisp < -63)) {
            size = 3;
            /* relocation on the GOTO that is inserted after this instruction */
            /* FIXME is a relocation needed here? */
            if (db != NULL && base != NULL) {
#if BIGENDIAN
                add_extnreloc(&db->relocs,base, value.value, REL_ABS, 10, 22, 1);
#else
                add_extnreloc_masked(&db->relocs,base, value.value, REL_ABS, 10, 6, 1, 0x3F0000);
                add_extnreloc_masked(&db->relocs,base, value.value, REL_ABS, 0, 16, 2, 0xFFFF);
#endif
            }
        } else {
            /* reverse the condition bit and remove the PC+2 offset since we will use an
             * immediate PC relative jump */
            *oc ^= 0010002;
            if ((db != NULL) && (base != NULL) && is_pc_reloc(base, sec)) {
                add_extnreloc(&db->relocs,base, value.value - 1, REL_PC, 10, 6, 0);
            } else {
                /* Relocation not needed, encode pc displacement directly in instruction */
                if (pcdisp < 0) {
                    /* negative branches are stored as sign bit + positive value */
                    *oc |= (-pcdisp) & 0x3f;
                    *oc |= 0x40;
                } else {
                    *oc |= pcdisp & 0x3f;
                }
            }
        }

        *extra = value.value;
        break;
    }

    case FIRMOV: {
        /* Like SO, but ON and OFF values are inverted) */
        uint16_t arg = encode_opb(ip->op[0], db, &value, sec, pc);
        *oc |= arg ^ 1;
        break;
    }

    case FIQ: {
        /* Like SO, but argument changes the second bit instead of the first */
        uint16_t arg = encode_opb(ip->op[0], db, &value, sec, pc);
        if (arg == 2) {
            /* "FIQ FIQ" is not allowed */
            cpu_error(2);
        }
        *oc |= arg << 1; /* Toggle second bit instead of first */
        break;
    }

    case NO: {
        /* No operands, easy! */
        break;
    }
    default:
      ierror(0);
      break;
  }

  return size;
}

size_t instruction_size(instruction *ip,section *sec,taddr pc)
{
  uint16_t oc;
  uint32_t extra;
  return process_instruction(copy_inst(ip), sec, pc, &oc, &extra, NULL);
}

dblock *eval_instruction(instruction *ip,section *sec,taddr pc)
{
  dblock *db = new_dblock();
  uint8_t *d = NULL;
  uint16_t oc;
  uint32_t extra;

  db->size = process_instruction(ip, sec, pc, &oc, &extra, db);
  d = db->data = mymalloc(db->size);

  /* write opcode */
  writebyte(d, oc);
  d += OCTETS(1);

  /* Special case for XJMP instructions: add the LJMP instruction after the short conditional jump */
  if (db->size >= 3) {
      writebyte(d, 0177200 | extra >> 16);
      d += OCTETS(1);
  }

  /* Write the address or immediate value, if any */
  if (db->size >= 2) {
      writebyte(d, extra);
      d += OCTETS(1);
  }

  return db;
}

dblock *eval_data(operand *op,size_t bitsize,section *sec,taddr pc)
{
  dblock *db = new_dblock();
  taddr val;

  /* TODO: handle packed bytes (with padding) and 32 bit values */
  if (bitsize != 16)
    cpu_error(4,bitsize);  /* data size not supported */

  db->size = bitsize / 16;
  db->data = mymalloc(db->size);

  if (!eval_expr(op->value,&val,sec,pc)) {
    symbol *base;
    int btype;
    
    modifier = 0;
    btype = find_base(op->value,&base,sec,pc);
    if (btype==BASE_OK || btype==BASE_PCREL) {
      add_extnreloc(&db->relocs,base,val,
                    btype==BASE_PCREL?REL_PC:REL_ABS,0,bitsize,0);
    }
    else
      general_error(38);  /* illegal relocation */
  }

  if (val<-0x8000 || val>0xffff)
    cpu_error(7, 16);  /* data doesn't fit into 16-bits */

  setval(0,db->data,db->size,val);
  return db;
}

int cpu_available(int idx)
{
    /* This can be used to manage different versions of the CPU with additional instructions */
    return 1;
}

int init_cpu(void)
{
  char r[4];
  int i;

  /* define register symbols */
  for (i=1; i<6; i++) {
    sprintf(r,"r%d",i);
    new_regsym(0,1,r,RTYPE_R,0,i);
  }
  new_regsym(0,1,"sp",RTYPE_R,0,0);
  new_regsym(0,1,"bp",RTYPE_R,0,5);
  new_regsym(0,1,"sr",RTYPE_R,0,6);
  new_regsym(0,1,"pc",RTYPE_R,0,7);

  return 1;
}

int cpu_args(char *p)
{
    return 0;
}
/*
 * cpu.c HANS cpu description file
 */

#include "vasm.h"

mnemonic mnemonics[] = {
  "add",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(0) },
  "sub",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(1) },
  "mul",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(2) },
  "sqrt",    { TargetReg,SourceReg1 },             { FORMR(3) },
  "div",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(4) },
  "mod",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(5) },
  "sla",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(6) },
  "sra",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(7) },
  "ce",      { TargetReg,SourceReg1,SourceReg2 },  { FORMR(8) },
  "cne",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(9) },
  "cg",      { TargetReg,SourceReg1,SourceReg2 },  { FORMR(10) },
  "cl",      { TargetReg,SourceReg1,SourceReg2 },  { FORMR(11) },
  "cgu",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(12) },
  "clu",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(13) },
  "cgeu",    { TargetReg,SourceReg2,SourceReg1 },  { FORMR(13) },
  "cleu",    { TargetReg,SourceReg2,SourceReg1 },  { FORMR(12) },
  "cge",     { TargetReg,SourceReg2,SourceReg1 },  { FORMR(11) },
  "cle",     { TargetReg,SourceReg2,SourceReg1 },  { FORMR(10) },
  "itof",    { TargetFloatReg,SourceReg1 },        { FORMR(14) },
  "uitof",   { TargetFloatReg,SourceReg1 },        { FORMR(15) },
  "not",     { TargetReg,SourceReg1 },             { FORMR(16) },
  "and",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(17) },
  "or",      { TargetReg,SourceReg1,SourceReg2 },  { FORMR(18) },
  "xor",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(19) },
  "xnor",    { TargetReg,SourceReg1,SourceReg2 },  { FORMR(20) },
  "sll",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(22) },
  "srl",     { TargetReg,SourceReg1,SourceReg2 },  { FORMR(23) },
  "add.s",   { TargetFloatReg,SourceFloatReg1,SourceFloatReg2 },  { FORMR(32) },
  "sub.s",   { TargetFloatReg,SourceFloatReg1,SourceFloatReg2 },  { FORMR(33) },
  "mul.s",   { TargetFloatReg,SourceFloatReg1,SourceFloatReg2 },  { FORMR(34) },
  "sqrt.s", { TargetFloatReg,SourceFloatReg1 },                  { FORMR(35) },
  "div.s",   { TargetFloatReg,SourceFloatReg1,SourceFloatReg2 },  { FORMR(36) },
  "ce.s",    { TargetReg,SourceFloatReg1,SourceFloatReg2 },       { FORMR(40) },
  "cne.s",   { TargetReg,SourceFloatReg1,SourceFloatReg2 },       { FORMR(41) },
  "cg.s",    { TargetReg,SourceFloatReg1,SourceFloatReg2 },       { FORMR(42) },
  "cl.s",    { TargetReg,SourceFloatReg1,SourceFloatReg2 },       { FORMR(43) },
  "cge.s",    { TargetReg,SourceFloatReg2,SourceFloatReg1 },       { FORMR(43) },
  "cle.s",    { TargetReg,SourceFloatReg2,SourceFloatReg1 },       { FORMR(42) },
  "ftoi",    { TargetReg,SourceFloatReg1 },                       { FORMR(46) },
  "ftoui",    { TargetReg,SourceFloatReg1 },                       { FORMR(47) },
  "addi",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(0) },
  "subi",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(1) },
  "muli",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(2) },
  "divi",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(4) },
  "modi",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(5) },
  "slai",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(6) },
  "srai",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(7) },
  "cei",     { TargetReg,SourceReg1,Immediate16 },  { FORMI(8) },
  "cnei",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(9) },
  "cgi",     { TargetReg,SourceReg1,Immediate16 },  { FORMI(10) },
  "cli",     { TargetReg,SourceReg1,Immediate16 },  { FORMI(11) },
  "cgei",    { TargetReg,SourceReg1,Immediate16Minus1 },  { FORMI(10) },
  "clei",    { TargetReg,SourceReg1,Immediate16Plus1 },  { FORMI(11) },
  "cgui",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(12) },
  "clui",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(13) },
  "cgeui",   { TargetReg,SourceReg1,Immediate16Minus1 },  { FORMI(12) },
  "cleui",   { TargetReg,SourceReg1,Immediate16Plus1 },  { FORMI(13) },
  "addis",   { TargetReg,SourceReg1,Immediate16 },  { FORMI(16) },
  "andi",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(17) },
  "ori",     { TargetReg,SourceReg1,Immediate16 },  { FORMI(18) },
  "xori",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(19) },
  "xnori",   { TargetReg,SourceReg1,Immediate16 },  { FORMI(20) },
  "slli",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(22) },
  "srli",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(23) },
  "load",    { TargetReg,SourceReg1,Immediate16 },  { FORMI(24) },
  "load.s",  { TargetFloatReg,SourceReg1,Immediate16 },            { FORMI(25) },
  "store",   { SourceReg1,TargetReg,Immediate16 },  { FORMI(26) },
  "store.s", { SourceReg1,TargetFloatReg,Immediate16 },            { FORMI(27) },
  "jreg",    { SourceReg1 },                        { FORMI(28) },
  "bez",     { SourceReg1, Immediate16Label },      { FORMI(29) },
  "bnez",    { SourceReg1, Immediate16Label },      { FORMI(30) },
  "jal",     { TargetReg, Immediate16Label },       { FORMI(31) },
  "jmp",     { Immediate26Label },                  { FORMJ(0) }
};
const int mnemonic_cnt = sizeof(mnemonics) / sizeof(mnemonics[0]);

const char* cpu_copyright = "vasm hans cpu backend 1.1 (c)2024 by Yannik Stamm";
const char* cpuname = "hans";
int bytespertaddr = 1;

operand* new_operand()
{
    operand* new = mymalloc(sizeof(*new));
    return new;
}

/*Returns the number of the register identifier, or a constant*/
static int parse_reg(char** start, int regtype)
{
    char* stringPos = *start;
    regsym* symbol;
    int registerIndex;
    unsigned int identifierLength;

    /*If string is an identifier...*/
    if (ISIDSTART(*stringPos))
    {
        stringPos++;
        while (ISIDCHAR(*stringPos)) stringPos++;

        /*...find that identifier and return the register number it references */
        identifierLength = stringPos - *start;

        if (symbol = find_regsym_nc(*start, identifierLength))
        {
            if (symbol->reg_type == regtype)
            {
                *start = stringPos;
                return symbol->reg_num;
            }
            
            return -1; /*In case register type is not the expected type, die*/
        }
    }

    /*...else if it is a number, check if it */
    /*is inside the valid interval and return it*/
    /* get register number */
    stringPos = *start;
    registerIndex = (int)parse_constexpr(&stringPos);  
    if (registerIndex >= 0 && registerIndex <= 31)
    {
        *start = stringPos;
        return registerIndex;
    }

    return -1;
}

static void resolve_high_low_label_reference
        (operand* operand, char* lastSymbolOfName)
{
    operand->labelType = DefaultLabel;
    if (*lastSymbolOfName != '@') return;

    if (*(lastSymbolOfName + 1) == 'l' || *(lastSymbolOfName + 1) == 'L')
    {
        operand->labelType = LowLabel;
    }
    else if (*(lastSymbolOfName + 1) == 'h' || *(lastSymbolOfName + 1) == 'H')
    {
        if (*(lastSymbolOfName + 2) == 'a' || *(lastSymbolOfName + 2) == 'A')
        {
            operand->labelType = HighAlgebraicLabel;
        }
        else
        {
            operand->labelType = HighLabel;
        }
    }

}

/*Sets operand->reg to the appropriate register and returns, */
/*if it was successful or not (returns PO_MATCH or PO_NOMATCH).*/
/*If the operand is an immediate-label, set operand->expr instead of operand->reg */
int parse_operand(char* start, int len, operand* operand, int requiredOperandType)
{
    /*Skips spaces*/
    start = skip(start);

    switch (requiredOperandType)
    {
    case SourceReg1:
    case SourceReg2:
    case TargetReg:
        if ((operand->reg = parse_reg(&start, RTYPE_R)) >= 0)
            return PO_MATCH;
        break;

    case SourceFloatReg1:
    case SourceFloatReg2:
    case TargetFloatReg:
        if ((operand->reg = parse_reg(&start, RTYPE_F)) >= 0)
            return PO_MATCH;
        break;

    case Immediate16:
        if (*start == '#')
            start = skip(start + 1);  /* skip optional '#' */
    case Data:
    case Immediate16Label:
    case Immediate26Label:
        operand->exp = parse_expr(&start);
        resolve_high_low_label_reference(operand, start);
        return PO_MATCH;
    case Immediate16Plus1:
        if (*start == '#')
            start = skip(start + 1);  /* skip optional '#' */
        operand->exp = make_expr(ADD, parse_expr(&start), number_expr(1));
        return PO_MATCH;
    case Immediate16Minus1:
        if (*start == '#')
            start = skip(start + 1);  /* skip optional '#' */
        operand->exp = make_expr(SUB, parse_expr(&start), number_expr(1));
        return PO_MATCH;
    }
    
    return PO_NOMATCH;
}


char* parse_cpu_special(char* s)
{
    return s;  /* nothing special */
}

/*Instructions are always one byte (= 32 Bit for this CPU),*/
/*so we can always return 1 here*/
size_t instruction_size(instruction* ip, section* sec, taddr pc)
{
    return 1;
}

/*Handles low and high labels*/
static uint32_t add_immediate(uint32_t opCode, taddr immediateValue, operand* operand)
{
    if (operand->labelType == HighLabel)
    {
        opCode |= (immediateValue & 0xffff0000) >> 16;
    }
    else if (operand->labelType == HighAlgebraicLabel)
    {
        uint16_t higherHalf = (immediateValue & 0xffff0000) >> 16;
        uint16_t lowerHalf = immediateValue & 0x0000ffff;
        if (lowerHalf > (1 << 15) - 1)
        {
            higherHalf += 1;
        }
        opCode |= higherHalf;
    }
    else
    {
        opCode |= immediateValue & 0x0000ffff;
    }

    return opCode;
}

int generateAddendum(operand *operand)
{
    int addendum;
    addendum = 0;
    if (operand->exp->type == ADD)
    {
        if (operand->exp->left->type == NUM)
            addendum = operand->exp->left->c.val;
        else if (operand->exp->right->type == NUM)
            addendum = operand->exp->right->c.val;
        else
            general_error(38); /*Illegal relocation*/
    }
    if (operand->exp->type == SUB)
        if (operand->exp->right->type == NUM && operand->exp->left->type == SYM)
            addendum = -operand->exp->right->c.val;
        else
            general_error(38); /*Illegal relocation*/
    return addendum;
}

/*Converts the received instruction into its final binary format*/
dblock* eval_instruction
        (instruction* instruction, section* section, taddr programCounter)
{
    mnemonic* mnemonic = &mnemonics[instruction->code];
    uint32_t opCode = mnemonic->ext.opcode;
    dblock* dataBlock = new_dblock();
    int i;

    dataBlock->size = 1;
    dataBlock->data = mymalloc(OCTETS(dataBlock->size));

    /* assemble all operands into the opcode */
    for (i = 0; i < MAX_OPERANDS; i++)
    {
        operand* operand = instruction->op[i];

        /*"A non-constant expression is based on a (defined or undefined) symbol*/
        /*(baseOfImmediate) and an addend (ImmediateValue)."*/
        symbol* baseOfImmediate = NULL;
        taddr immediateValue; 

        if (operand == NULL)
            break;  /* no more operands */
        if (operand->exp != NULL)
        {
            /*If immediateValue depends on a symbol, find that symbol*/
            if (!eval_expr(operand->exp, &immediateValue, section, programCounter))
            {
                int result = find_base(operand->exp, &baseOfImmediate, section, programCounter);
                if (result != BASE_OK)
                    general_error(38);
            }
        }


        switch (mnemonic->operand_type[i])
        {
        case TargetReg:
        case TargetFloatReg:
            opCode |= (operand->reg & 31) << 21;
            break;
        case SourceReg1:
        case SourceFloatReg1:
            opCode |= (operand->reg & 31) << 16;
            break;
        case SourceReg2:
        case SourceFloatReg2:
            opCode |= (operand->reg & 31) << 11;
            break;
        case Immediate16Minus1:
            if (strcmp(mnemonic->name, "cgei") == 0 && immediateValue < -0x8000 || immediateValue > 0x7FFF)
                cpu_error(4, immediateValue + 1, "[-32767:32768]", mnemonic->name);
            if (strcmp(mnemonic->name, "cgeui") == 0 && immediateValue < 0x0000 || immediateValue > 0xFFFF)
                cpu_error(4, immediateValue + 1, "[1:65536]", mnemonic->name);
        case Immediate16Plus1:
            if (strcmp(mnemonic->name, "clei") == 0 && immediateValue < -0x8000 || immediateValue > 0x7FFF)
                cpu_error(4, immediateValue - 1, "[-32769:32766]", mnemonic->name);
            if (strcmp(mnemonic->name, "cleui") == 0 && immediateValue < -0x0000 || immediateValue > 0xFFFF)
                cpu_error(4, immediateValue - 1, "[-1:65534]", mnemonic->name);
        case Immediate16:
            if (baseOfImmediate != NULL)
            {
                rlist* newReloc;
                int addendum;
                /* external label or label from a different section needs reloc */
                int mask;
                if (operand->labelType == HighLabel)
                {
                    mask = 0xFFFF0000;
                }
                else if (operand->labelType == HighAlgebraicLabel)
                {
                    rlist* reloc;
                    mask = 0xFFFF0000;

                    reloc = add_extnreloc(&dataBlock->relocs,
                        baseOfImmediate, 0, REL_ABS, 16, 16, 0);
                    ((nreloc*)reloc->reloc)->mask = 0x8000;
                }
                else if (operand->labelType == LowLabel)
                {
                    mask = 0xFFFF;
                }
                else
                    mask = 0;

                addendum = generateAddendum(operand);
                newReloc = add_extnreloc(&dataBlock->relocs,
                    baseOfImmediate, addendum, REL_ABS, 16, 16, 0);
                ((nreloc*)newReloc->reloc)->mask = mask;
            }
            /*Only throw warning if immediate out of range of 16 Bit signed/unsigned int*/
            if (immediateValue < -0x8000 || immediateValue > 0xFFFF)
                cpu_error(1, immediateValue);


            opCode = add_immediate(opCode, immediateValue, operand);
            break;
        case Immediate16Label:
            /*If operand contains a label*/
            if (baseOfImmediate != NULL)
            {
                /*If symbol is extern*/
                if (is_pc_reloc(baseOfImmediate, section))
                {
                    int addendum;
                    addendum = generateAddendum(operand);
                    /* external label or label from a different section needs reloc */
                    add_extnreloc(&dataBlock->relocs,
                        baseOfImmediate, addendum, REL_PC, 16, 16, 0);
                }
            }
            immediateValue -= programCounter + 1;

            /*Only throw warning if immediate out of range of 16 Bit signed int*/
            if (immediateValue < -0x8000 || immediateValue > 0x7FFF)
                cpu_error(2, immediateValue);
            /*Should not be used with @l, @h, @ha so no add_immediate*/
            opCode |= immediateValue & 0xffff;
            break;
        case Immediate26Label:
            /*If operand contains a label*/
            if (baseOfImmediate != NULL)
            {
                /*If symbol is extern*/
                if (is_pc_reloc(baseOfImmediate, section))
                {
                    int addendum;
                    addendum = generateAddendum(operand);
                    /* external label or label from a different section needs reloc */
                    add_extnreloc(&dataBlock->relocs,
                        baseOfImmediate, addendum, REL_PC, 6, 26, 0);
                }
            }
            immediateValue -= programCounter + 1;
            if (immediateValue < -0x2000000 || immediateValue > 0x1FFFFFF)
                cpu_error(3, immediateValue);

            /*Should not be used with @l, @h, @ha so no add_immediate*/
            opCode |= immediateValue & 0x3ffffff; 
            break;
        case Data:
            ierror(0);  /* should be handled by eval_data() */
            break;
        }
    }

    /* write opcode - endianness doesn't matter */
    setval(1, dataBlock->data, dataBlock->size, opCode);
    return dataBlock;
}

/*Converts the received data operand into its final binary format*/
dblock* eval_data
    (operand* operand, size_t bitsize, section* section, taddr programCounter)
{
    dblock* dataBlock = new_dblock();
    taddr value;
    if (bitsize % BITSPERBYTE)
        cpu_error(0, bitsize);  /* data size not supported */

    dataBlock->size = bitsize / BITSPERBYTE;
    dataBlock->data = mymalloc(OCTETS(dataBlock->size));

    /* evaluate expression, get baseOfImmediate and type for relocations */
    if (!eval_expr(operand->exp, &value, section, programCounter))
    {
        symbol* baseOfImmediate;
        int baseType;

        baseType = find_base(operand->exp, &baseOfImmediate, section, programCounter);
        if (baseType == BASE_OK || baseType == BASE_PCREL)
        {
            int addendum;
            addendum = generateAddendum(operand);
            add_extnreloc(&dataBlock->relocs, baseOfImmediate, addendum,
                baseType == BASE_PCREL ? REL_PC : REL_ABS, 0, bitsize, 0);
        }
        else
            general_error(38);  /* illegal relocation */
    }

    setval(1, dataBlock->data, dataBlock->size, value);
    return dataBlock;
}


int init_cpu()
{
    char r[4];
    int i;

    /* define register symbols */
    for (i = 0; i < 32; i++)
    {
        sprintf(r, "r%d", i);
        new_regsym(0, 1, r, RTYPE_R, 0, i);
        r[0] = 'f';
        new_regsym(0, 1, r, RTYPE_F, 0, i);
    }

    return 1;
}


int cpu_args(char* p)
{
    return 0;
}
