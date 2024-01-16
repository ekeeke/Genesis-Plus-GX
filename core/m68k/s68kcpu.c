/* ======================================================================== */
/*                            SUB 68K CORE                                  */
/* ======================================================================== */

extern int scd_68k_irq_ack(int level);

#define m68ki_cpu s68k
#define MUL (4)

/* ======================================================================== */
/* ================================ INCLUDES ============================== */
/* ======================================================================== */

#ifndef BUILD_TABLES
#include "s68ki_cycles.h"
#endif

#include "s68kconf.h"
#include "m68kcpu.h"
#include "m68kops.h"

/* ======================================================================== */
/* ================================= DATA ================================= */
/* ======================================================================== */

#ifdef BUILD_TABLES
static unsigned char s68ki_cycles[0x10000];
#endif
static int irq_latency;

/* IRQ priority */
static const uint8 irq_level[0x40] = 
{
  0, 1, 2, 2, 3, 3, 3, 3,
  4, 4, 4, 4, 4, 4, 4, 4,
  5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5,
  6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6
};

m68ki_cpu_core s68k;


/* ======================================================================== */
/* =============================== CALLBACKS ============================== */
/* ======================================================================== */

/* Default callbacks used if the callback hasn't been set yet, or if the
 * callback is set to NULL
 */

#if M68K_EMULATE_INT_ACK == OPT_ON
/* Interrupt acknowledge */
static int default_int_ack_callback(int int_level)
{
  CPU_INT_LEVEL = 0;
  return M68K_INT_ACK_AUTOVECTOR;
}
#endif

#if M68K_EMULATE_RESET == OPT_ON
/* Called when a reset instruction is executed */
static void default_reset_instr_callback(void)
{
}
#endif

#if M68K_TAS_HAS_CALLBACK == OPT_ON
/* Called when a tas instruction is executed */
static int default_tas_instr_callback(void)
{
  return 1; // allow writeback
}
#endif

#if M68K_EMULATE_FC == OPT_ON
/* Called every time there's bus activity (read/write to/from memory */
static void default_set_fc_callback(unsigned int new_fc)
{
}
#endif


/* ======================================================================== */
/* ================================= API ================================== */
/* ======================================================================== */

/* Access the internals of the CPU */
unsigned int s68k_get_reg(m68k_register_t regnum)
{
  switch(regnum)
  {
    case M68K_REG_D0:  return m68ki_cpu.dar[0];
    case M68K_REG_D1:  return m68ki_cpu.dar[1];
    case M68K_REG_D2:  return m68ki_cpu.dar[2];
    case M68K_REG_D3:  return m68ki_cpu.dar[3];
    case M68K_REG_D4:  return m68ki_cpu.dar[4];
    case M68K_REG_D5:  return m68ki_cpu.dar[5];
    case M68K_REG_D6:  return m68ki_cpu.dar[6];
    case M68K_REG_D7:  return m68ki_cpu.dar[7];
    case M68K_REG_A0:  return m68ki_cpu.dar[8];
    case M68K_REG_A1:  return m68ki_cpu.dar[9];
    case M68K_REG_A2:  return m68ki_cpu.dar[10];
    case M68K_REG_A3:  return m68ki_cpu.dar[11];
    case M68K_REG_A4:  return m68ki_cpu.dar[12];
    case M68K_REG_A5:  return m68ki_cpu.dar[13];
    case M68K_REG_A6:  return m68ki_cpu.dar[14];
    case M68K_REG_A7:  return m68ki_cpu.dar[15];
    case M68K_REG_PC:  return MASK_OUT_ABOVE_32(m68ki_cpu.pc);
    case M68K_REG_SR:  return  m68ki_cpu.t1_flag        |
                  (m68ki_cpu.s_flag << 11)              |
                   m68ki_cpu.int_mask                   |
                  ((m68ki_cpu.x_flag & XFLAG_SET) >> 4) |
                  ((m68ki_cpu.n_flag & NFLAG_SET) >> 4) |
                  ((!m68ki_cpu.not_z_flag) << 2)        |
                  ((m68ki_cpu.v_flag & VFLAG_SET) >> 6) |
                  ((m68ki_cpu.c_flag & CFLAG_SET) >> 8);
    case M68K_REG_SP:  return m68ki_cpu.dar[15];
    case M68K_REG_USP:  return m68ki_cpu.s_flag ? m68ki_cpu.sp[0] : m68ki_cpu.dar[15];
    case M68K_REG_ISP:  return m68ki_cpu.s_flag ? m68ki_cpu.dar[15] : m68ki_cpu.sp[4];
#if M68K_EMULATE_PREFETCH
    case M68K_REG_PREF_ADDR:  return m68ki_cpu.pref_addr;
    case M68K_REG_PREF_DATA:  return m68ki_cpu.pref_data;
#endif
    case M68K_REG_IR:  return m68ki_cpu.ir;
    default:      return 0;
  }
}

void s68k_set_reg(m68k_register_t regnum, unsigned int value)
{
  switch(regnum)
  {
    case M68K_REG_D0:  REG_D[0] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_D1:  REG_D[1] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_D2:  REG_D[2] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_D3:  REG_D[3] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_D4:  REG_D[4] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_D5:  REG_D[5] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_D6:  REG_D[6] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_D7:  REG_D[7] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A0:  REG_A[0] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A1:  REG_A[1] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A2:  REG_A[2] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A3:  REG_A[3] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A4:  REG_A[4] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A5:  REG_A[5] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A6:  REG_A[6] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A7:  REG_A[7] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_PC:  m68ki_jump(MASK_OUT_ABOVE_32(value)); return;
    case M68K_REG_SR:  m68ki_set_sr(value); return;
    case M68K_REG_SP:  REG_SP = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_USP:  if(FLAG_S)
                REG_USP = MASK_OUT_ABOVE_32(value);
              else
                REG_SP = MASK_OUT_ABOVE_32(value);
              return;
    case M68K_REG_ISP:  if(FLAG_S)
                REG_SP = MASK_OUT_ABOVE_32(value);
              else
                REG_ISP = MASK_OUT_ABOVE_32(value);
              return;
    case M68K_REG_IR:  REG_IR = MASK_OUT_ABOVE_16(value); return;
#if M68K_EMULATE_PREFETCH
    case M68K_REG_PREF_ADDR:  CPU_PREF_ADDR = MASK_OUT_ABOVE_32(value); return;
#endif
    default:      return;
  }
}

/* Set the callbacks */
#if M68K_EMULATE_INT_ACK == OPT_ON
void s68k_set_int_ack_callback(int  (*callback)(int int_level))
{
  CALLBACK_INT_ACK = callback ? callback : default_int_ack_callback;
}
#endif

#if M68K_EMULATE_RESET == OPT_ON
void s68k_set_reset_instr_callback(void  (*callback)(void))
{
  CALLBACK_RESET_INSTR = callback ? callback : default_reset_instr_callback;
}
#endif

#if M68K_TAS_HAS_CALLBACK == OPT_ON
void s68k_set_tas_instr_callback(int  (*callback)(void))
{
  CALLBACK_TAS_INSTR = callback ? callback : default_tas_instr_callback;
}
#endif

#if M68K_EMULATE_FC == OPT_ON
void s68k_set_fc_callback(void  (*callback)(unsigned int new_fc))
{
  CALLBACK_SET_FC = callback ? callback : default_set_fc_callback;
}
#endif

extern void error(char *format, ...);
extern uint16 v_counter;

/* update IRQ level according to triggered interrupts */
void s68k_update_irq(unsigned int mask)
{
  /* Get IRQ level (6 interrupt lines) */
  mask = irq_level[mask];

  /* Set IRQ level */
  CPU_INT_LEVEL = mask << 8;
  
#ifdef LOG_SCD
  error("[%d][%d] s68k IRQ Level = %d(0x%02x) (%x)\n", v_counter, s68k.cycles, CPU_INT_LEVEL>>8,FLAG_INT_MASK,s68k.pc);
#endif
}

void s68k_run(unsigned int cycles) 
{
  /* Make sure CPU is not already ahead */
  if (s68k.cycles >= cycles)
  {
    return;
  }

  /* Check interrupt mask to process IRQ if needed */
  m68ki_check_interrupts();

  /* Make sure we're not stopped */
  if (CPU_STOPPED)
  {
    s68k.cycles = cycles;
    return;
  }

  /* Save end cycles count for when CPU is stopped */
  s68k.cycle_end = cycles;

  /* Return point for when we have an address error (TODO: use goto) */
  m68ki_set_address_error_trap() /* auto-disable (see m68kcpu.h) */

#ifdef LOG_SCD
  error("[%d][%d] s68k run to %d cycles (%x), irq mask = %x (%x)\n", v_counter, s68k.cycles, cycles, s68k.pc,FLAG_INT_MASK, CPU_INT_LEVEL);
#endif
 
  while (s68k.cycles < cycles)
  {
    /* Set tracing accodring to T1. */
    m68ki_trace_t1() /* auto-disable (see m68kcpu.h) */

    /* Set the address space for reads */
    m68ki_use_data_space() /* auto-disable (see m68kcpu.h) */

    /* Save current instruction PC */
    s68k.prev_pc = REG_PC;

    /* Decode next instruction */
    REG_IR = m68ki_read_imm_16();

    /* Execute instruction */
    m68ki_instruction_jump_table[REG_IR]();
    USE_CYCLES(CYC_INSTRUCTION[REG_IR]);

    /* Trace m68k_exception, if necessary */
    m68ki_exception_if_trace(); /* auto-disable (see m68kcpu.h) */
  }
}


int s68k_cycles(void)
{
  return CYC_INSTRUCTION[REG_IR];
}

void s68k_init(void)
{
#ifdef BUILD_TABLES
  static uint emulation_initialized = 0;

  /* The first call to this function initializes the opcode handler jump table */
  if(!emulation_initialized)
  {
    m68ki_build_opcode_table();
    emulation_initialized = 1;
  }
#endif

#ifdef M68K_OVERCLOCK_SHIFT
  s68k.cycle_ratio = 1 << M68K_OVERCLOCK_SHIFT;
#endif

#if M68K_EMULATE_INT_ACK == OPT_ON
  s68k_set_int_ack_callback(NULL);
#endif
#if M68K_EMULATE_RESET == OPT_ON
  s68k_set_reset_instr_callback(NULL);
#endif
#if M68K_TAS_HAS_CALLBACK == OPT_ON
  s68k_set_tas_instr_callback(NULL);
#endif
#if M68K_EMULATE_FC == OPT_ON
  s68k_set_fc_callback(NULL);
#endif
}

/* Pulse the RESET line on the CPU */
void s68k_pulse_reset(void)
{
  /* Clear all stop levels */
  CPU_STOPPED = 0;
#if M68K_EMULATE_ADDRESS_ERROR
  CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET;
#endif

  /* Turn off tracing */
  FLAG_T1 = 0;
  m68ki_clear_trace()

  /* Interrupt mask to level 7 */
  FLAG_INT_MASK = 0x0700;
  CPU_INT_LEVEL = 0;
  irq_latency = 0;

  /* Go to supervisor mode */
  m68ki_set_s_flag(SFLAG_SET);

  /* Invalidate the prefetch queue */
#if M68K_EMULATE_PREFETCH
  /* Set to arbitrary number since our first fetch is from 0 */
  CPU_PREF_ADDR = 0x1000;
#endif /* M68K_EMULATE_PREFETCH */

  /* Read the initial stack pointer and program counter */
  m68ki_jump(0);
  REG_SP = m68ki_read_imm_32();
  REG_PC = m68ki_read_imm_32();
  m68ki_jump(REG_PC);

#if M68K_EMULATE_ADDRESS_ERROR
  CPU_RUN_MODE = RUN_MODE_NORMAL;
#endif

  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_RESET]);
}

void s68k_pulse_halt(void)
{
  /* Pulse the HALT line on the CPU */
  CPU_STOPPED |= STOP_LEVEL_HALT;
}

void s68k_clear_halt(void)
{
  /* Clear the HALT line on the CPU */
  CPU_STOPPED &= ~STOP_LEVEL_HALT;
}

void s68k_pulse_wait(unsigned int address, unsigned int write_access)
{
  /* Check CPU is not already waiting for /DTACK */
  if (!(CPU_STOPPED & STOP_LEVEL_WAIT))
  {
    /* Hold the DTACK line on the CPU */
    CPU_STOPPED |= STOP_LEVEL_WAIT;

    /* End CPU execution */
    s68k.cycles = s68k.cycle_end - s68k_cycles();

    /* Save CPU address registers */
    s68k.prev_ar[0] = s68k.dar[8+0];
    s68k.prev_ar[1] = s68k.dar[8+1];
    s68k.prev_ar[2] = s68k.dar[8+2];
    s68k.prev_ar[3] = s68k.dar[8+3];
    s68k.prev_ar[4] = s68k.dar[8+4];
    s68k.prev_ar[5] = s68k.dar[8+5];
    s68k.prev_ar[6] = s68k.dar[8+6];
    s68k.prev_ar[7] = s68k.dar[8+7];

    /* Detect address register(s) pre-decrement/post-increment done by MOVE/MOVEA instruction */
    if ((s68k.ir >= 0x1000) && (s68k.ir < 0x4000))
    {
      /* MOVE/MOVEA instructions operand sizes */
      static const int mov_instr_sizes[4] = {0, 1, 4, 2};

      if ((s68k.ir & 0x38) == 0x18)
      {
        /* revert source address register post-increment */
        s68k.prev_ar[s68k.ir&0x07] -= mov_instr_sizes[(s68k.ir>>12)&0x03];
      }
      else if ((s68k.ir & 0x38) == 0x20)
      {
        /* revert source address register pre-decrement */
        s68k.prev_ar[s68k.ir&0x07] += mov_instr_sizes[(s68k.ir>>12)&0x03];
      }

      /* only check destination address register post-increment/pre-decrement in case of halting on write access */
      if (write_access)
      {
        if ((s68k.ir & 0x01c0) == 0x00c0)
        {
          /* revert destination address register post-increment */
          s68k.prev_ar[(s68k.ir>>9)&0x07] -= mov_instr_sizes[(s68k.ir>>12)&0x03];
        }
        else if ((s68k.ir & 0x01c0) == 0x0100)
        {
          /* revert destination address register pre-decrement */
          s68k.prev_ar[(s68k.ir>>9)&0x07] += mov_instr_sizes[(s68k.ir>>12)&0x03];
        }
      }
    }
    else
    {
      /* Other instructions operand sizes */
      static const int def_instr_sizes[4] = {1, 2, 4, 2};

      /* Detect address register(s) pre-decrement done by ABCD/SBCD instruction */
      if ((s68k.ir & 0xb1f8) == 0x8108)
      {
        /* revert source address register pre-decrement (byte operands only) */
        s68k.prev_ar[s68k.ir&0x07] += 1;

        /* only revert destination address register pre-decrement in case of halting on destination address access (byte operands only) */
        if (address == s68k.prev_ar[(s68k.ir>>9)&0x07])
        {
          s68k.prev_ar[(s68k.ir>>9)&0x07] += 1;
        }
      }

      /* Detect address register(s) pre-decrement done by ADDX/SUBX instruction */
      else if (((s68k.ir & 0xb1f8) == 0x9108) || ((s68k.ir & 0xb1f8) == 0x9148) || ((s68k.ir & 0xb1f8) == 0x9188))
      {
        /* revert source address register pre-decrement */
        s68k.prev_ar[s68k.ir&0x07] += def_instr_sizes[(s68k.ir>>6)&0x03];

        /* only revert destination address register pre-decrement in case of halting on destination address access */
        if (address == s68k.prev_ar[(s68k.ir>>9)&0x07])
        {
          s68k.prev_ar[(s68k.ir>>9)&0x07] += def_instr_sizes[(s68k.ir>>6)&0x03];
        }
      }

      /* Detect address register(s) post-increment done by CMPM instruction */
      else if ((s68k.ir & 0xf138) == 0xb108)
      {
        /* revert source address register post-increment */
        s68k.prev_ar[s68k.ir&0x07] -= def_instr_sizes[(s68k.ir>>6)&0x03];

        /* only revert destination address register post-increment in case of halting on destination address access */
        if (address == s68k.prev_ar[(s68k.ir>>9)&0x07])
        {
          s68k.prev_ar[(s68k.ir>>9)&0x07] -= def_instr_sizes[(s68k.ir>>6)&0x03];
        }
      }

      /* Detect address register post-increment or pre-increment done by other instruction */
      else if (((s68k.ir & 0x38) == 0x18) || ((s68k.ir & 0x38) == 0x20))
      {
        int size;

        /* autodetect MOVEM instruction (no address register modification needed as post-increment/pre-decrement is done after memory access) */
        if ((s68k.ir & 0xfb80) == 0x4880)
        {
          size = 0;
        }

        /* autodetect instruction with fixed byte operand (and not covered by generic size field value) */
        else if (((s68k.ir & 0xf100) == 0x0100) || /* BTST, BCHG, BCLR, BSET (dynamic) */
                 ((s68k.ir & 0xff00) == 0x0800) || /* BTST, BCHG, BCLR, BSET (static) */
                 ((s68k.ir & 0xffc0) == 0x4ac0) || /* TAS */
                 ((s68k.ir & 0xf0c0) == 0x50c0))   /* Scc */
        {
          size = 1;
        }

        /* autodetect instruction with fixed word operand (and not covered by generic size field value) */
        else if ((s68k.ir & 0xf1c0) == 0x4180) /* CHK */
        {
          size = 2;
        }

        /* autodetect instruction with either word or long operand (not covered by generic size field value) */
        else if (((s68k.ir & 0xb0c0) == 0x90c0) || /* SUBA, ADDA*/
                 ((s68k.ir & 0xf0c0) == 0xb0c0))   /* CMPA */
        {
          size = (s68k.ir & 0x100) ? 4 : 2;
        }

        /* default operand size */
        else
        {
          size = def_instr_sizes[(s68k.ir>>6)&0x03];
        }

        if (s68k.ir & 0x08)
        {
          /* revert source address register post-increment */
          s68k.prev_ar[s68k.ir&0x07] -= size;
        }
        else
        {
          /* revert source address register pre-decrement */
          s68k.prev_ar[s68k.ir&0x07] += size;
        }
      }
    }
  }
}

void s68k_clear_wait(void)
{
  /* check CPU is waiting for DTACK */
  if (CPU_STOPPED & STOP_LEVEL_WAIT)
  {
    /* Assert the DTACK line on the CPU */
    CPU_STOPPED &= ~STOP_LEVEL_WAIT;

    /* Rollback to previously held instruction */
    s68k.pc = s68k.prev_pc;

    /* Restore CPU address registers */
    s68k.dar[8+0] = s68k.prev_ar[0];
    s68k.dar[8+1] = s68k.prev_ar[1];
    s68k.dar[8+2] = s68k.prev_ar[2];
    s68k.dar[8+3] = s68k.prev_ar[3];
    s68k.dar[8+4] = s68k.prev_ar[4];
    s68k.dar[8+5] = s68k.prev_ar[5];
    s68k.dar[8+6] = s68k.prev_ar[6];
    s68k.dar[8+7] = s68k.prev_ar[7];
  }
}

/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */
