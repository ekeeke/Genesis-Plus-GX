/* ======================================================================== */
/* ========================= LICENSING & COPYRIGHT ======================== */
/* ======================================================================== */
/*
 *                                  MUSASHI
 *                                Version 3.32
 *
 * A portable Motorola M680x0 processor emulation engine.
 * Copyright Karl Stenerud.  All rights reserved.
 *
 * This code may be freely used for non-commercial purposes as long as this
 * copyright notice remains unaltered in the source code and any binary files
 * containing this code in compiled form.
 *
 * All other licensing terms must be negotiated with the author
 * (Karl Stenerud).
 *
 * The latest version of this code can be obtained at:
 * http://kstenerud.cjb.net
 */

 /* Modified by Eke-Eke for Genesis Plus GX:

    - removed unused stuff to reduce memory usage / optimize execution (multiple CPU types support, NMI support, ...)
    - moved stuff to compile statically in a single object file
    - implemented support for global cycle count (shared by 68k & Z80 CPU)
    - added support for interrupt latency (Sesame's Street Counting Cafe, Fatal Rewind)
    - added proper cycle use on reset
    - added cycle accurate timings for MUL/DIV instructions (thanks to Jorge Cwik !) 
    - fixed undocumented flags for DIV instructions (Blood Shot)
    
  */

#ifndef M68KCPU__HEADER
#define M68KCPU__HEADER

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "m68k.h"

#if M68K_EMULATE_ADDRESS_ERROR
#include <setjmp.h>
#endif /* M68K_EMULATE_ADDRESS_ERROR */

/* ======================================================================== */
/* ==================== ARCHITECTURE-DEPENDANT DEFINES ==================== */
/* ======================================================================== */

/* Check for > 32bit sizes */
#if UINT_MAX > 0xffffffff
  #define M68K_INT_GT_32_BIT  1
#else
  #define M68K_INT_GT_32_BIT  0
#endif

/* Data types used in this emulation core */
#undef sint8
#undef sint16
#undef sint32
#undef sint64
#undef uint8
#undef uint16
#undef uint32
#undef uint64
#undef sint
#undef uint

#define sint8  signed   char      /* ASG: changed from char to signed char */
#define sint16 signed   short
#define sint32 signed   int      /* AWJ: changed from long to int */
#define uint8  unsigned char
#define uint16 unsigned short
#define uint32 unsigned int      /* AWJ: changed from long to int */

/* signed and unsigned int must be at least 32 bits wide */
#define sint   signed   int
#define uint   unsigned int


#if M68K_USE_64_BIT
#define sint64 signed   long long
#define uint64 unsigned long long
#else
#define sint64 sint32
#define uint64 uint32
#endif /* M68K_USE_64_BIT */



/* Allow for architectures that don't have 8-bit sizes */
#if UCHAR_MAX == 0xff
  #define MAKE_INT_8(A) (sint8)(A)
#else
  #undef  sint8
  #define sint8  signed   int
  #undef  uint8
  #define uint8  unsigned int
  INLINE sint MAKE_INT_8(uint value)
  {
    return (value & 0x80) ? value | ~0xff : value & 0xff;
  }
#endif /* UCHAR_MAX == 0xff */


/* Allow for architectures that don't have 16-bit sizes */
#if USHRT_MAX == 0xffff
  #define MAKE_INT_16(A) (sint16)(A)
#else
  #undef  sint16
  #define sint16 signed   int
  #undef  uint16
  #define uint16 unsigned int
  INLINE sint MAKE_INT_16(uint value)
  {
    return (value & 0x8000) ? value | ~0xffff : value & 0xffff;
  }
#endif /* USHRT_MAX == 0xffff */


/* Allow for architectures that don't have 32-bit sizes */
#if UINT_MAX == 0xffffffff
  #define MAKE_INT_32(A) (sint32)(A)
#else
  #undef  sint32
  #define sint32  signed   int
  #undef  uint32
  #define uint32  unsigned int
  INLINE sint MAKE_INT_32(uint value)
  {
    return (value & 0x80000000) ? value | ~0xffffffff : value & 0xffffffff;
  }
#endif /* UINT_MAX == 0xffffffff */




/* ======================================================================== */
/* ============================ GENERAL DEFINES =========================== */
/* ======================================================================== */

/* Exception Vectors handled by emulation */
#define EXCEPTION_RESET                    0
#define EXCEPTION_BUS_ERROR                2 /* This one is not emulated! */
#define EXCEPTION_ADDRESS_ERROR            3 /* This one is partially emulated (doesn't stack a proper frame yet) */
#define EXCEPTION_ILLEGAL_INSTRUCTION      4
#define EXCEPTION_ZERO_DIVIDE              5
#define EXCEPTION_CHK                      6
#define EXCEPTION_TRAPV                    7
#define EXCEPTION_PRIVILEGE_VIOLATION      8
#define EXCEPTION_TRACE                    9
#define EXCEPTION_1010                    10
#define EXCEPTION_1111                    11
#define EXCEPTION_FORMAT_ERROR            14
#define EXCEPTION_UNINITIALIZED_INTERRUPT 15
#define EXCEPTION_SPURIOUS_INTERRUPT      24
#define EXCEPTION_INTERRUPT_AUTOVECTOR    24
#define EXCEPTION_TRAP_BASE               32

/* Function codes set by CPU during data/address bus activity */
#define FUNCTION_CODE_USER_DATA          1
#define FUNCTION_CODE_USER_PROGRAM       2
#define FUNCTION_CODE_SUPERVISOR_DATA    5
#define FUNCTION_CODE_SUPERVISOR_PROGRAM 6
#define FUNCTION_CODE_CPU_SPACE          7

/* Different ways to stop the CPU */
#define STOP_LEVEL_STOP 1
#define STOP_LEVEL_HALT 2

/* Used for 68000 address error processing */
#if M68K_EMULATE_ADDRESS_ERROR
#define INSTRUCTION_YES 0
#define INSTRUCTION_NO  0x08
#define MODE_READ       0x10
#define MODE_WRITE      0

#define RUN_MODE_NORMAL          0
#define RUN_MODE_BERR_AERR_RESET 1
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

/* ======================================================================== */
/* ================================ MACROS ================================ */
/* ======================================================================== */


/* ---------------------------- General Macros ---------------------------- */

/* Bit Isolation Macros */
#define BIT_0(A)  ((A) & 0x00000001)
#define BIT_1(A)  ((A) & 0x00000002)
#define BIT_2(A)  ((A) & 0x00000004)
#define BIT_3(A)  ((A) & 0x00000008)
#define BIT_4(A)  ((A) & 0x00000010)
#define BIT_5(A)  ((A) & 0x00000020)
#define BIT_6(A)  ((A) & 0x00000040)
#define BIT_7(A)  ((A) & 0x00000080)
#define BIT_8(A)  ((A) & 0x00000100)
#define BIT_9(A)  ((A) & 0x00000200)
#define BIT_A(A)  ((A) & 0x00000400)
#define BIT_B(A)  ((A) & 0x00000800)
#define BIT_C(A)  ((A) & 0x00001000)
#define BIT_D(A)  ((A) & 0x00002000)
#define BIT_E(A)  ((A) & 0x00004000)
#define BIT_F(A)  ((A) & 0x00008000)
#define BIT_10(A) ((A) & 0x00010000)
#define BIT_11(A) ((A) & 0x00020000)
#define BIT_12(A) ((A) & 0x00040000)
#define BIT_13(A) ((A) & 0x00080000)
#define BIT_14(A) ((A) & 0x00100000)
#define BIT_15(A) ((A) & 0x00200000)
#define BIT_16(A) ((A) & 0x00400000)
#define BIT_17(A) ((A) & 0x00800000)
#define BIT_18(A) ((A) & 0x01000000)
#define BIT_19(A) ((A) & 0x02000000)
#define BIT_1A(A) ((A) & 0x04000000)
#define BIT_1B(A) ((A) & 0x08000000)
#define BIT_1C(A) ((A) & 0x10000000)
#define BIT_1D(A) ((A) & 0x20000000)
#define BIT_1E(A) ((A) & 0x40000000)
#define BIT_1F(A) ((A) & 0x80000000)

/* Get the most significant bit for specific sizes */
#define GET_MSB_8(A)  ((A) & 0x80)
#define GET_MSB_9(A)  ((A) & 0x100)
#define GET_MSB_16(A) ((A) & 0x8000)
#define GET_MSB_17(A) ((A) & 0x10000)
#define GET_MSB_32(A) ((A) & 0x80000000)
#if M68K_USE_64_BIT
#define GET_MSB_33(A) ((A) & 0x100000000)
#endif /* M68K_USE_64_BIT */

/* Isolate nibbles */
#define LOW_NIBBLE(A)  ((A) & 0x0f)
#define HIGH_NIBBLE(A) ((A) & 0xf0)

/* These are used to isolate 8, 16, and 32 bit sizes */
#define MASK_OUT_ABOVE_2(A)  ((A) & 3)
#define MASK_OUT_ABOVE_8(A)  ((A) & 0xff)
#define MASK_OUT_ABOVE_16(A) ((A) & 0xffff)
#define MASK_OUT_BELOW_2(A)  ((A) & ~3)
#define MASK_OUT_BELOW_8(A)  ((A) & ~0xff)
#define MASK_OUT_BELOW_16(A) ((A) & ~0xffff)

/* No need to mask if we are 32 bit */
#if M68K_INT_GT_32_BIT || M68K_USE_64_BIT
  #define MASK_OUT_ABOVE_32(A) ((A) & 0xffffffff)
  #define MASK_OUT_BELOW_32(A) ((A) & ~0xffffffff)
#else
  #define MASK_OUT_ABOVE_32(A) (A)
  #define MASK_OUT_BELOW_32(A) 0
#endif /* M68K_INT_GT_32_BIT || M68K_USE_64_BIT */

/* Simulate address lines of 68k family */
#define ADDRESS_68K(A) ((A)&CPU_ADDRESS_MASK)


/* Shift & Rotate Macros. */
#define LSL(A, C) ((A) << (C))
#define LSR(A, C) ((A) >> (C))

/* Some > 32-bit optimizations */
#if M68K_INT_GT_32_BIT
  /* Shift left and right */
  #define LSR_32(A, C) ((A) >> (C))
  #define LSL_32(A, C) ((A) << (C))
#else
  /* We have to do this because the morons at ANSI decided that shifts
     * by >= data size are undefined.
     */
  #define LSR_32(A, C) ((C) < 32 ? (A) >> (C) : 0)
  #define LSL_32(A, C) ((C) < 32 ? (A) << (C) : 0)
#endif /* M68K_INT_GT_32_BIT */

#if M68K_USE_64_BIT
  #define LSL_32_64(A, C) ((A) << (C))
  #define LSR_32_64(A, C) ((A) >> (C))
  #define ROL_33_64(A, C) (LSL_32_64(A, C) | LSR_32_64(A, 33-(C)))
  #define ROR_33_64(A, C) (LSR_32_64(A, C) | LSL_32_64(A, 33-(C)))
#endif /* M68K_USE_64_BIT */

#define ROL_8(A, C)      MASK_OUT_ABOVE_8(LSL(A, C) | LSR(A, 8-(C)))
#define ROL_9(A, C)                      (LSL(A, C) | LSR(A, 9-(C)))
#define ROL_16(A, C)    MASK_OUT_ABOVE_16(LSL(A, C) | LSR(A, 16-(C)))
#define ROL_17(A, C)                     (LSL(A, C) | LSR(A, 17-(C)))
#define ROL_32(A, C)    MASK_OUT_ABOVE_32(LSL_32(A, C) | LSR_32(A, 32-(C)))
#define ROL_33(A, C)                     (LSL_32(A, C) | LSR_32(A, 33-(C)))

#define ROR_8(A, C)      MASK_OUT_ABOVE_8(LSR(A, C) | LSL(A, 8-(C)))
#define ROR_9(A, C)                      (LSR(A, C) | LSL(A, 9-(C)))
#define ROR_16(A, C)    MASK_OUT_ABOVE_16(LSR(A, C) | LSL(A, 16-(C)))
#define ROR_17(A, C)                     (LSR(A, C) | LSL(A, 17-(C)))
#define ROR_32(A, C)    MASK_OUT_ABOVE_32(LSR_32(A, C) | LSL_32(A, 32-(C)))
#define ROR_33(A, C)                     (LSR_32(A, C) | LSL_32(A, 33-(C)))



/* ------------------------------ CPU Access ------------------------------ */

/* Access the CPU registers */
#define REG_DA           m68ki_cpu.dar /* easy access to data and address regs */
#define REG_D            m68ki_cpu.dar
#define REG_A            (m68ki_cpu.dar+8)
#define REG_PC           m68ki_cpu.pc
#define REG_SP_BASE      m68ki_cpu.sp
#define REG_USP          m68ki_cpu.sp[0]
#define REG_ISP          m68ki_cpu.sp[4]
#define REG_SP           m68ki_cpu.dar[15]
#define REG_IR           m68ki_cpu.ir

#define FLAG_T1          m68ki_cpu.t1_flag
#define FLAG_S           m68ki_cpu.s_flag
#define FLAG_X           m68ki_cpu.x_flag
#define FLAG_N           m68ki_cpu.n_flag
#define FLAG_Z           m68ki_cpu.not_z_flag
#define FLAG_V           m68ki_cpu.v_flag
#define FLAG_C           m68ki_cpu.c_flag
#define FLAG_INT_MASK    m68ki_cpu.int_mask

#define CPU_INT_LEVEL    m68ki_cpu.int_level /* ASG: changed from CPU_INTS_PENDING */
#define CPU_STOPPED      m68ki_cpu.stopped
#if M68K_EMULATE_PREFETCH
#define CPU_PREF_ADDR    m68ki_cpu.pref_addr
#define CPU_PREF_DATA    m68ki_cpu.pref_data
#endif
#define CPU_ADDRESS_MASK  0x00ffffff
#if M68K_EMULATE_ADDRESS_ERROR
#define CPU_INSTR_MODE   m68ki_cpu.instr_mode
#define CPU_RUN_MODE     m68ki_cpu.run_mode
#endif

#define CYC_INSTRUCTION   m68ki_cycles
#define CYC_EXCEPTION     m68ki_exception_cycle_table
#define CYC_BCC_NOTAKE_B  ( -2 * 7)
#define CYC_BCC_NOTAKE_W  (  2 * 7)
#define CYC_DBCC_F_NOEXP  ( -2 * 7)
#define CYC_DBCC_F_EXP    (  2 * 7)
#define CYC_SCC_R_TRUE    (  2 * 7)
#define CYC_MOVEM_W       (  4 * 7)
#define CYC_MOVEM_L       (  8 * 7)
#define CYC_SHIFT         (  2 * 7)
#define CYC_RESET         (132 * 7)

#if M68K_EMULATE_INT_ACK == OPT_ON
#define CALLBACK_INT_ACK      m68ki_cpu.int_ack_callback
#endif
#if M68K_EMULATE_RESET == OPT_ON
#define CALLBACK_RESET_INSTR  m68ki_cpu.reset_instr_callback
#endif
#if M68K_TAS_HAS_CALLBACK == OPT_ON
#define CALLBACK_TAS_INSTR    m68ki_cpu.tas_instr_callback
#endif
#if M68K_EMULATE_FC == OPT_ON
#define CALLBACK_SET_FC       m68ki_cpu.set_fc_callback
#endif


/* ----------------------------- Configuration ---------------------------- */

/* These defines are dependant on the configuration defines in m68kconf.h */

/* Enable or disable callback functions */
#if M68K_EMULATE_INT_ACK
  #if M68K_EMULATE_INT_ACK == OPT_SPECIFY_HANDLER
    #define m68ki_int_ack(A) M68K_INT_ACK_CALLBACK(A);
  #else
    #define m68ki_int_ack(A) CALLBACK_INT_ACK(A);
  #endif
#else
  /* Default action is to used autovector mode, which is most common */
  #define m68ki_int_ack(A) M68K_INT_ACK_AUTOVECTOR
#endif /* M68K_EMULATE_INT_ACK */

#if M68K_EMULATE_RESET
  #if M68K_EMULATE_RESET == OPT_SPECIFY_HANDLER
    #define m68ki_output_reset() M68K_RESET_CALLBACK();
  #else
    #define m68ki_output_reset() CALLBACK_RESET_INSTR();
  #endif
#else
  #define m68ki_output_reset()
#endif /* M68K_EMULATE_RESET */

#if M68K_TAS_HAS_CALLBACK
  #if M68K_TAS_HAS_CALLBACK == OPT_SPECIFY_HANDLER
    #define m68ki_tas_callback() M68K_TAS_CALLBACK()
  #else
    #define m68ki_tas_callback() CALLBACK_TAS_INSTR()
  #endif
#else
  #define m68ki_tas_callback() 0
#endif /* M68K_TAS_HAS_CALLBACK */


/* Enable or disable function code emulation */
#if M68K_EMULATE_FC
  #if M68K_EMULATE_FC == OPT_SPECIFY_HANDLER
    #define m68ki_set_fc(A) M68K_SET_FC_CALLBACK(A);
  #else
    #define m68ki_set_fc(A) CALLBACK_SET_FC(A);
  #endif
  #define m68ki_use_data_space() m68ki_address_space = FUNCTION_CODE_USER_DATA;
  #define m68ki_use_program_space() m68ki_address_space = FUNCTION_CODE_USER_PROGRAM;
  #define m68ki_get_address_space() m68ki_address_space
#else
  #define m68ki_set_fc(A)
  #define m68ki_use_data_space()
  #define m68ki_use_program_space()
  #define m68ki_get_address_space() FUNCTION_CODE_USER_DATA
#endif /* M68K_EMULATE_FC */


/* Enable or disable trace emulation */
#if M68K_EMULATE_TRACE
  /* Initiates trace checking before each instruction (t1) */
  #define m68ki_trace_t1() m68ki_tracing = FLAG_T1;
  /* Clear all tracing */
  #define m68ki_clear_trace() m68ki_tracing = 0;
  /* Cause a trace exception if we are tracing */
  #define m68ki_exception_if_trace() if(m68ki_tracing) m68ki_exception_trace();
#else
  #define m68ki_trace_t1()
  #define m68ki_clear_trace()
  #define m68ki_exception_if_trace()
#endif /* M68K_EMULATE_TRACE */


/* Enable or disable Address error emulation */
#if M68K_EMULATE_ADDRESS_ERROR
  #define m68ki_set_address_error_trap() \
    if(setjmp(m68ki_aerr_trap) != 0) \
    { \
      m68ki_exception_address_error(); \
    }

  #define m68ki_check_address_error(ADDR, WRITE_MODE, FC) \
    if((ADDR)&1) \
    { \
      if (emulate_address_error) \
      { \
        m68ki_aerr_address = ADDR; \
        m68ki_aerr_write_mode = WRITE_MODE; \
        m68ki_aerr_fc = FC; \
        longjmp(m68ki_aerr_trap, 1); \
      } \
    }
#else
  #define m68ki_set_address_error_trap()
  #define m68ki_check_address_error(ADDR, WRITE_MODE, FC)
#endif /* M68K_ADDRESS_ERROR */


/* -------------------------- EA / Operand Access ------------------------- */

/*
 * The general instruction format follows this pattern:
 * .... XXX. .... .YYY
 * where XXX is register X and YYY is register Y
 */

/* Data Register Isolation */
#define DX (REG_D[(REG_IR >> 9) & 7])
#define DY (REG_D[REG_IR & 7])

/* Address Register Isolation */
#define AX (REG_A[(REG_IR >> 9) & 7])
#define AY (REG_A[REG_IR & 7])

/* Effective Address Calculations */
#define EA_AY_AI_8()   AY                                    /* address register indirect */
#define EA_AY_AI_16()  EA_AY_AI_8()
#define EA_AY_AI_32()  EA_AY_AI_8()
#define EA_AY_PI_8()   (AY++)                                /* postincrement (size = byte) */
#define EA_AY_PI_16()  ((AY+=2)-2)                           /* postincrement (size = word) */
#define EA_AY_PI_32()  ((AY+=4)-4)                           /* postincrement (size = long) */
#define EA_AY_PD_8()   (--AY)                                /* predecrement (size = byte) */
#define EA_AY_PD_16()  (AY-=2)                               /* predecrement (size = word) */
#define EA_AY_PD_32()  (AY-=4)                               /* predecrement (size = long) */
#define EA_AY_DI_8()   (AY+MAKE_INT_16(m68ki_read_imm_16())) /* displacement */
#define EA_AY_DI_16()  EA_AY_DI_8()
#define EA_AY_DI_32()  EA_AY_DI_8()
#define EA_AY_IX_8()   m68ki_get_ea_ix(AY)                   /* indirect + index */
#define EA_AY_IX_16()  EA_AY_IX_8()
#define EA_AY_IX_32()  EA_AY_IX_8()

#define EA_AX_AI_8()   AX
#define EA_AX_AI_16()  EA_AX_AI_8()
#define EA_AX_AI_32()  EA_AX_AI_8()
#define EA_AX_PI_8()   (AX++)
#define EA_AX_PI_16()  ((AX+=2)-2)
#define EA_AX_PI_32()  ((AX+=4)-4)
#define EA_AX_PD_8()   (--AX)
#define EA_AX_PD_16()  (AX-=2)
#define EA_AX_PD_32()  (AX-=4)
#define EA_AX_DI_8()   (AX+MAKE_INT_16(m68ki_read_imm_16()))
#define EA_AX_DI_16()  EA_AX_DI_8()
#define EA_AX_DI_32()  EA_AX_DI_8()
#define EA_AX_IX_8()   m68ki_get_ea_ix(AX)
#define EA_AX_IX_16()  EA_AX_IX_8()
#define EA_AX_IX_32()  EA_AX_IX_8()

#define EA_A7_PI_8()   ((REG_A[7]+=2)-2)
#define EA_A7_PD_8()   (REG_A[7]-=2)

#define EA_AW_8()      MAKE_INT_16(m68ki_read_imm_16())      /* absolute word */
#define EA_AW_16()     EA_AW_8()
#define EA_AW_32()     EA_AW_8()
#define EA_AL_8()      m68ki_read_imm_32()                   /* absolute long */
#define EA_AL_16()     EA_AL_8()
#define EA_AL_32()     EA_AL_8()
#define EA_PCDI_8()    m68ki_get_ea_pcdi()                   /* pc indirect + displacement */
#define EA_PCDI_16()   EA_PCDI_8()
#define EA_PCDI_32()   EA_PCDI_8()
#define EA_PCIX_8()    m68ki_get_ea_pcix()                   /* pc indirect + index */
#define EA_PCIX_16()   EA_PCIX_8()
#define EA_PCIX_32()   EA_PCIX_8()


#define OPER_I_8()     m68ki_read_imm_8()
#define OPER_I_16()    m68ki_read_imm_16()
#define OPER_I_32()    m68ki_read_imm_32()


/* --------------------------- Status Register ---------------------------- */

/* Flag Calculation Macros */
#define CFLAG_8(A) (A)
#define CFLAG_16(A) ((A)>>8)

#if M68K_INT_GT_32_BIT
  #define CFLAG_ADD_32(S, D, R) ((R)>>24)
  #define CFLAG_SUB_32(S, D, R) ((R)>>24)
#else
  #define CFLAG_ADD_32(S, D, R) (((S & D) | (~R & (S | D)))>>23)
  #define CFLAG_SUB_32(S, D, R) (((S & R) | (~D & (S | R)))>>23)
#endif /* M68K_INT_GT_32_BIT */

#define VFLAG_ADD_8(S, D, R) ((S^R) & (D^R))
#define VFLAG_ADD_16(S, D, R) (((S^R) & (D^R))>>8)
#define VFLAG_ADD_32(S, D, R) (((S^R) & (D^R))>>24)

#define VFLAG_SUB_8(S, D, R) ((S^D) & (R^D))
#define VFLAG_SUB_16(S, D, R) (((S^D) & (R^D))>>8)
#define VFLAG_SUB_32(S, D, R) (((S^D) & (R^D))>>24)

#define NFLAG_8(A) (A)
#define NFLAG_16(A) ((A)>>8)
#define NFLAG_32(A) ((A)>>24)
#define NFLAG_64(A) ((A)>>56)

#define ZFLAG_8(A) MASK_OUT_ABOVE_8(A)
#define ZFLAG_16(A) MASK_OUT_ABOVE_16(A)
#define ZFLAG_32(A) MASK_OUT_ABOVE_32(A)


/* Flag values */
#define NFLAG_SET   0x80
#define NFLAG_CLEAR 0
#define CFLAG_SET   0x100
#define CFLAG_CLEAR 0
#define XFLAG_SET   0x100
#define XFLAG_CLEAR 0
#define VFLAG_SET   0x80
#define VFLAG_CLEAR 0
#define ZFLAG_SET   0
#define ZFLAG_CLEAR 0xffffffff
#define SFLAG_SET   4
#define SFLAG_CLEAR 0

/* Turn flag values into 1 or 0 */
#define XFLAG_AS_1() ((FLAG_X>>8)&1)
#define NFLAG_AS_1() ((FLAG_N>>7)&1)
#define VFLAG_AS_1() ((FLAG_V>>7)&1)
#define ZFLAG_AS_1() (!FLAG_Z)
#define CFLAG_AS_1() ((FLAG_C>>8)&1)


/* Conditions */
#define COND_CS() (FLAG_C&0x100)
#define COND_CC() (!COND_CS())
#define COND_VS() (FLAG_V&0x80)
#define COND_VC() (!COND_VS())
#define COND_NE() FLAG_Z
#define COND_EQ() (!COND_NE())
#define COND_MI() (FLAG_N&0x80)
#define COND_PL() (!COND_MI())
#define COND_LT() ((FLAG_N^FLAG_V)&0x80)
#define COND_GE() (!COND_LT())
#define COND_HI() (COND_CC() && COND_NE())
#define COND_LS() (COND_CS() || COND_EQ())
#define COND_GT() (COND_GE() && COND_NE())
#define COND_LE() (COND_LT() || COND_EQ())

/* Reversed conditions */
#define COND_NOT_CS() COND_CC()
#define COND_NOT_CC() COND_CS()
#define COND_NOT_VS() COND_VC()
#define COND_NOT_VC() COND_VS()
#define COND_NOT_NE() COND_EQ()
#define COND_NOT_EQ() COND_NE()
#define COND_NOT_MI() COND_PL()
#define COND_NOT_PL() COND_MI()
#define COND_NOT_LT() COND_GE()
#define COND_NOT_GE() COND_LT()
#define COND_NOT_HI() COND_LS()
#define COND_NOT_LS() COND_HI()
#define COND_NOT_GT() COND_LE()
#define COND_NOT_LE() COND_GT()

/* Not real conditions, but here for convenience */
#define COND_XS() (FLAG_X&0x100)
#define COND_XC() (!COND_XS)


/* Get the condition code register */
#define m68ki_get_ccr() ((COND_XS() >> 4) | \
             (COND_MI() >> 4) | \
             (COND_EQ() << 2) | \
             (COND_VS() >> 6) | \
             (COND_CS() >> 8))

/* Get the status register */
#define m68ki_get_sr() ( FLAG_T1  | \
            (FLAG_S        << 11) | \
             FLAG_INT_MASK        | \
             m68ki_get_ccr())



/* ---------------------------- Cycle Counting ---------------------------- */

#define USE_CYCLES(A)    mcycles_68k += (A)
#define END_CYCLES(A)    mcycles_68k = (A)


/* ----------------------------- Read / Write ----------------------------- */

/* Read from the current address space */
#define m68ki_read_8(A)  m68ki_read_8_fc (A, FLAG_S | m68ki_get_address_space())
#define m68ki_read_16(A) m68ki_read_16_fc(A, FLAG_S | m68ki_get_address_space())
#define m68ki_read_32(A) m68ki_read_32_fc(A, FLAG_S | m68ki_get_address_space())

/* Write to the current data space */
#define m68ki_write_8(A, V)  m68ki_write_8_fc (A, FLAG_S | FUNCTION_CODE_USER_DATA, V)
#define m68ki_write_16(A, V) m68ki_write_16_fc(A, FLAG_S | FUNCTION_CODE_USER_DATA, V)
#define m68ki_write_32(A, V) m68ki_write_32_fc(A, FLAG_S | FUNCTION_CODE_USER_DATA, V)

/* map read immediate 8 to read immediate 16 */
#define m68ki_read_imm_8() MASK_OUT_ABOVE_8(m68ki_read_imm_16())

/* Map PC-relative reads */
#define m68ki_read_pcrel_8(A) m68k_read_pcrelative_8(A)
#define m68ki_read_pcrel_16(A) m68k_read_pcrelative_16(A)
#define m68ki_read_pcrel_32(A) m68k_read_pcrelative_32(A)

/* Read from the program space */
#define m68ki_read_program_8(A)   m68ki_read_8_fc(A, FLAG_S | FUNCTION_CODE_USER_PROGRAM)
#define m68ki_read_program_16(A)   m68ki_read_16_fc(A, FLAG_S | FUNCTION_CODE_USER_PROGRAM)
#define m68ki_read_program_32(A)   m68ki_read_32_fc(A, FLAG_S | FUNCTION_CODE_USER_PROGRAM)

/* Read from the data space */
#define m68ki_read_data_8(A)   m68ki_read_8_fc(A, FLAG_S | FUNCTION_CODE_USER_DATA)
#define m68ki_read_data_16(A)   m68ki_read_16_fc(A, FLAG_S | FUNCTION_CODE_USER_DATA)
#define m68ki_read_data_32(A)   m68ki_read_32_fc(A, FLAG_S | FUNCTION_CODE_USER_DATA)



/* ======================================================================== */
/* =============================== PROTOTYPES ============================= */
/* ======================================================================== */

typedef struct
{
  uint dar[16];      /* Data and Address Registers */
  uint pc;           /* Program Counter */
  uint sp[5];        /* User and Interrupt Stack Pointers */
  uint ir;           /* Instruction Register */
  uint t1_flag;      /* Trace 1 */
  uint s_flag;       /* Supervisor */
  uint x_flag;       /* Extend */
  uint n_flag;       /* Negative */
  uint not_z_flag;   /* Zero, inverted for speedups */
  uint v_flag;       /* Overflow */
  uint c_flag;       /* Carry */
  uint int_mask;     /* I0-I2 */
  uint int_level;    /* State of interrupt pins IPL0-IPL2 -- ASG: changed from ints_pending */
  uint stopped;      /* Stopped state */
#if M68K_EMULATE_PREFETCH
  uint pref_addr;    /* Last prefetch address */
  uint pref_data;    /* Data in the prefetch queue */
#endif
#if M68K_EMULATE_ADDRESS_ERROR
  uint instr_mode;   /* Stores whether we are in instruction mode or group 0/1 exception mode */
  uint run_mode;     /* Stores whether we are processing a reset, bus error, address error, or something else */
#endif

  /* Callbacks to host */
#if M68K_EMULATE_INT_ACK == OPT_ON
  int  (*int_ack_callback)(int int_line);           /* Interrupt Acknowledge */
#endif
#if M68K_EMULATE_RESET == OPT_ON
  void (*reset_instr_callback)(void);               /* Called when a RESET instruction is encountered */
#endif
#if M68K_TAS_HAS_CALLBACK == OPT_ON
  int  (*tas_instr_callback)(void);                 /* Called when a TAS instruction is encountered, allows / disallows writeback */
#endif
#if M68K_EMULATE_FC == OPT_ON
  void (*set_fc_callback)(unsigned int new_fc);     /* Called when the CPU function code changes */
#endif
} m68ki_cpu_core;

/* The CPU core */
m68ki_cpu_core m68ki_cpu;

/* Prevent static data from being redefined when this file is included */
#ifndef _MEM68K_H_

#if M68K_EMULATE_TRACE
static uint m68ki_tracing;
#endif /* M68K_EMULATE_TRACE */

#if M68K_EMULATE_FC
static uint m68ki_address_space;
#endif /* M68K_EMULATE_FC */

#if M68K_EMULATE_ADDRESS_ERROR
static jmp_buf m68ki_aerr_trap;
static uint m68ki_aerr_address;
static uint m68ki_aerr_write_mode;
static uint m68ki_aerr_fc;
int emulate_address_error;
#endif /* M68K_EMULATE_ADDRESS_ERROR */

/* Current end cycle */
static unsigned int end_cycles;

/* Cycles used by CPU instructions */
#ifdef BUILD_TABLES
static unsigned char m68ki_cycles[0x10000];
#else
#include "m68ki_cycles.h"
#endif

/* Used by shift & rotate instructions */
static const uint8 m68ki_shift_8_table[65] =
{
  0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff
};

static const uint16 m68ki_shift_16_table[65] =
{
  0x0000, 0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00,
  0xff80, 0xffc0, 0xffe0, 0xfff0, 0xfff8, 0xfffc, 0xfffe, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff
};

static const uint m68ki_shift_32_table[65] =
{
  0x00000000, 0x80000000, 0xc0000000, 0xe0000000, 0xf0000000, 0xf8000000,
  0xfc000000, 0xfe000000, 0xff000000, 0xff800000, 0xffc00000, 0xffe00000,
  0xfff00000, 0xfff80000, 0xfffc0000, 0xfffe0000, 0xffff0000, 0xffff8000,
  0xffffc000, 0xffffe000, 0xfffff000, 0xfffff800, 0xfffffc00, 0xfffffe00,
  0xffffff00, 0xffffff80, 0xffffffc0, 0xffffffe0, 0xfffffff0, 0xfffffff8,
  0xfffffffc, 0xfffffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};


/* Number of clock cycles to use for exception processing.
 * I used 4 for any vectors that are undocumented for processing times.
 */
static const uint16 m68ki_exception_cycle_table[256] =
{
     40*7, /*  0: Reset - Initial Stack Pointer                      */
      4*7, /*  1: Reset - Initial Program Counter                    */
     50*7, /*  2: Bus Error                             (unemulated) */
     50*7, /*  3: Address Error                         (unemulated) */
     34*7, /*  4: Illegal Instruction                                */
     38*7, /*  5: Divide by Zero -- ASG: changed from 42             */
     40*7, /*  6: CHK -- ASG: chanaged from 44                       */
     34*7, /*  7: TRAPV                                              */
     34*7, /*  8: Privilege Violation                                */
     34*7, /*  9: Trace                                              */
      4*7, /* 10: 1010                                               */
      4*7, /* 11: 1111                                               */
      4*7, /* 12: RESERVED                                           */
      4*7, /* 13: Coprocessor Protocol Violation        (unemulated) */
      4*7, /* 14: Format Error                                       */
     44*7, /* 15: Uninitialized Interrupt                            */
      4*7, /* 16: RESERVED                                           */
      4*7, /* 17: RESERVED                                           */
      4*7, /* 18: RESERVED                                           */
      4*7, /* 19: RESERVED                                           */
      4*7, /* 20: RESERVED                                           */
      4*7, /* 21: RESERVED                                           */
      4*7, /* 22: RESERVED                                           */
      4*7, /* 23: RESERVED                                           */
     44*7, /* 24: Spurious Interrupt                                 */
     44*7, /* 25: Level 1 Interrupt Autovector                       */
     44*7, /* 26: Level 2 Interrupt Autovector                       */
     44*7, /* 27: Level 3 Interrupt Autovector                       */
     44*7, /* 28: Level 4 Interrupt Autovector                       */
     44*7, /* 29: Level 5 Interrupt Autovector                       */
     44*7, /* 30: Level 6 Interrupt Autovector                       */
     44*7, /* 31: Level 7 Interrupt Autovector                       */
     34*7, /* 32: TRAP #0 -- ASG: chanaged from 38                   */
     34*7, /* 33: TRAP #1                                            */
     34*7, /* 34: TRAP #2                                            */
     34*7, /* 35: TRAP #3                                            */
     34*7, /* 36: TRAP #4                                            */
     34*7, /* 37: TRAP #5                                            */
     34*7, /* 38: TRAP #6                                            */
     34*7, /* 39: TRAP #7                                            */
     34*7, /* 40: TRAP #8                                            */
     34*7, /* 41: TRAP #9                                            */
     34*7, /* 42: TRAP #10                                           */
     34*7, /* 43: TRAP #11                                           */
     34*7, /* 44: TRAP #12                                           */
     34*7, /* 45: TRAP #13                                           */
     34*7, /* 46: TRAP #14                                           */
     34*7, /* 47: TRAP #15                                           */
      4*7, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
      4*7, /* 49: FP Inexact Result                     (unemulated) */
      4*7, /* 50: FP Divide by Zero                     (unemulated) */
      4*7, /* 51: FP Underflow                          (unemulated) */
      4*7, /* 52: FP Operand Error                      (unemulated) */
      4*7, /* 53: FP Overflow                           (unemulated) */
      4*7, /* 54: FP Signaling NAN                      (unemulated) */
      4*7, /* 55: FP Unimplemented Data Type            (unemulated) */
      4*7, /* 56: MMU Configuration Error               (unemulated) */
      4*7, /* 57: MMU Illegal Operation Error           (unemulated) */
      4*7, /* 58: MMU Access Level Violation Error      (unemulated) */
      4*7, /* 59: RESERVED                                           */
      4*7, /* 60: RESERVED                                           */
      4*7, /* 61: RESERVED                                           */
      4*7, /* 62: RESERVED                                           */
      4*7, /* 63: RESERVED                                           */
         /* 64-255: User Defined                                   */
      4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,
      4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,
      4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,
      4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,
      4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,
      4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7,4*7
};

/* Read data immediately after the program counter */
INLINE uint m68ki_read_imm_16(void);
INLINE uint m68ki_read_imm_32(void);

/* Read data with specific function code */
INLINE uint m68ki_read_8_fc  (uint address, uint fc);
INLINE uint m68ki_read_16_fc (uint address, uint fc);
INLINE uint m68ki_read_32_fc (uint address, uint fc);

/* Write data with specific function code */
INLINE void m68ki_write_8_fc (uint address, uint fc, uint value);
INLINE void m68ki_write_16_fc(uint address, uint fc, uint value);
INLINE void m68ki_write_32_fc(uint address, uint fc, uint value);

/* Indexed and PC-relative ea fetching */
INLINE uint m68ki_get_ea_pcdi(void);
INLINE uint m68ki_get_ea_pcix(void);
INLINE uint m68ki_get_ea_ix(uint An);

/* Operand fetching */
INLINE uint OPER_AY_AI_8(void);
INLINE uint OPER_AY_AI_16(void);
INLINE uint OPER_AY_AI_32(void);
INLINE uint OPER_AY_PI_8(void);
INLINE uint OPER_AY_PI_16(void);
INLINE uint OPER_AY_PI_32(void);
INLINE uint OPER_AY_PD_8(void);
INLINE uint OPER_AY_PD_16(void);
INLINE uint OPER_AY_PD_32(void);
INLINE uint OPER_AY_DI_8(void);
INLINE uint OPER_AY_DI_16(void);
INLINE uint OPER_AY_DI_32(void);
INLINE uint OPER_AY_IX_8(void);
INLINE uint OPER_AY_IX_16(void);
INLINE uint OPER_AY_IX_32(void);

INLINE uint OPER_AX_AI_8(void);
INLINE uint OPER_AX_AI_16(void);
INLINE uint OPER_AX_AI_32(void);
INLINE uint OPER_AX_PI_8(void);
INLINE uint OPER_AX_PI_16(void);
INLINE uint OPER_AX_PI_32(void);
INLINE uint OPER_AX_PD_8(void);
INLINE uint OPER_AX_PD_16(void);
INLINE uint OPER_AX_PD_32(void);
INLINE uint OPER_AX_DI_8(void);
INLINE uint OPER_AX_DI_16(void);
INLINE uint OPER_AX_DI_32(void);
INLINE uint OPER_AX_IX_8(void);
INLINE uint OPER_AX_IX_16(void);
INLINE uint OPER_AX_IX_32(void);

INLINE uint OPER_A7_PI_8(void);
INLINE uint OPER_A7_PD_8(void);

INLINE uint OPER_AW_8(void);
INLINE uint OPER_AW_16(void);
INLINE uint OPER_AW_32(void);
INLINE uint OPER_AL_8(void);
INLINE uint OPER_AL_16(void);
INLINE uint OPER_AL_32(void);
INLINE uint OPER_PCDI_8(void);
INLINE uint OPER_PCDI_16(void);
INLINE uint OPER_PCDI_32(void);
INLINE uint OPER_PCIX_8(void);
INLINE uint OPER_PCIX_16(void);
INLINE uint OPER_PCIX_32(void);

/* Stack operations */
INLINE void m68ki_push_16(uint value);
INLINE void m68ki_push_32(uint value);
INLINE uint m68ki_pull_16(void);
INLINE uint m68ki_pull_32(void);

/* Program flow operations */
INLINE void m68ki_jump(uint new_pc);
INLINE void m68ki_jump_vector(uint vector);
INLINE void m68ki_branch_8(uint offset);
INLINE void m68ki_branch_16(uint offset);
INLINE void m68ki_branch_32(uint offset);

/* Status register operations. */
INLINE void m68ki_set_s_flag(uint value);            /* Only bit 2 of value should be set (i.e. 4 or 0) */
INLINE void m68ki_set_ccr(uint value);               /* set the condition code register */
INLINE void m68ki_set_sr(uint value);                /* set the status register */

/* Exception processing */
INLINE uint m68ki_init_exception(void);              /* Initial exception processing */
INLINE void m68ki_stack_frame_3word(uint pc, uint sr); /* Stack various frame types */
#if M68K_EMULATE_ADDRESS_ERROR
INLINE void m68ki_stack_frame_buserr(uint sr);
#endif
INLINE void m68ki_exception_trap(uint vector);
INLINE void m68ki_exception_trapN(uint vector);
#if M68K_EMULATE_TRACE
INLINE void m68ki_exception_trace(void);
#endif
static void m68ki_exception_privilege_violation(void); /* do not inline in order to reduce function size and allow inlining of read/write functions by the compile */
INLINE void m68ki_exception_1010(void);
INLINE void m68ki_exception_1111(void);
INLINE void m68ki_exception_illegal(void);
#if M68K_EMULATE_ADDRESS_ERROR
INLINE void m68ki_exception_address_error(void);
#endif
INLINE void m68ki_exception_interrupt(uint int_level);
INLINE void m68ki_check_interrupts(void);            /* ASG: check for interrupts */

/* External ressources (specific to Genesis Plus GX core) */
extern int vdp_68k_irq_ack(int int_level);
extern uint32 mcycles_68k;

/* ======================================================================== */
/* =========================== UTILITY FUNCTIONS ========================== */
/* ======================================================================== */


/* ---------------------------- Read Immediate ---------------------------- */

/* Handles all immediate reads, does address error check, function code setting,
 * and prefetching if they are enabled in m68kconf.h
 */
INLINE uint m68ki_read_imm_16(void)
{
  m68ki_set_fc(FLAG_S | FUNCTION_CODE_USER_PROGRAM) /* auto-disable (see m68kcpu.h) */
#if M68K_CHECK_PC_ADDRESS_ERROR
  m68ki_check_address_error(REG_PC, MODE_READ, FLAG_S | FUNCTION_CODE_USER_PROGRAM) /* auto-disable (see m68kcpu.h) */
#endif
#if M68K_EMULATE_PREFETCH
  if(MASK_OUT_BELOW_2(REG_PC) != CPU_PREF_ADDR)
  {
    CPU_PREF_ADDR = MASK_OUT_BELOW_2(REG_PC);
    CPU_PREF_DATA = m68k_read_immediate_32(CPU_PREF_ADDR);
  }
  REG_PC += 2;
  return MASK_OUT_ABOVE_16(CPU_PREF_DATA >> ((2-((REG_PC-2)&2))<<3));
#else
  uint pc = REG_PC;
  REG_PC += 2;
  return m68k_read_immediate_16(pc);
#endif /* M68K_EMULATE_PREFETCH */
}

INLINE uint m68ki_read_imm_32(void)
{
#if M68K_EMULATE_PREFETCH
  uint temp_val;

  m68ki_set_fc(FLAG_S | FUNCTION_CODE_USER_PROGRAM) /* auto-disable (see m68kcpu.h) */
#if M68K_CHECK_PC_ADDRESS_ERROR
  m68ki_check_address_error(REG_PC, MODE_READ, FLAG_S | FUNCTION_CODE_USER_PROGRAM) /* auto-disable (see m68kcpu.h) */
#endif
  if(MASK_OUT_BELOW_2(REG_PC) != CPU_PREF_ADDR)
  {
    CPU_PREF_ADDR = MASK_OUT_BELOW_2(REG_PC);
    CPU_PREF_DATA = m68k_read_immediate_32(CPU_PREF_ADDR);
  }
  temp_val = CPU_PREF_DATA;
  REG_PC += 2;
  if(MASK_OUT_BELOW_2(REG_PC) != CPU_PREF_ADDR)
  {
    CPU_PREF_ADDR = MASK_OUT_BELOW_2(REG_PC);
    CPU_PREF_DATA = m68k_read_immediate_32(CPU_PREF_ADDR);
    temp_val = MASK_OUT_ABOVE_32((temp_val << 16) | (CPU_PREF_DATA >> 16));
  }
  REG_PC += 2;

  return temp_val;
#else
  m68ki_set_fc(FLAG_S | FUNCTION_CODE_USER_PROGRAM) /* auto-disable (see m68kcpu.h) */
#if M68K_CHECK_PC_ADDRESS_ERROR
  m68ki_check_address_error(REG_PC, MODE_READ, FLAG_S | FUNCTION_CODE_USER_PROGRAM) /* auto-disable (see m68kcpu.h) */
#endif
  uint pc = REG_PC;
  REG_PC += 4;
  return m68k_read_immediate_32(pc);
#endif /* M68K_EMULATE_PREFETCH */
}



/* ------------------------- Top level read/write ------------------------- */

/* Handles all memory accesses (except for immediate reads if they are
 * configured to use separate functions in m68kconf.h).
 * All memory accesses must go through these top level functions.
 * These functions will also check for address error and set the function
 * code if they are enabled in m68kconf.h.
 */
INLINE uint m68ki_read_8_fc(uint address, uint fc)
{
  _m68k_memory_map *temp;

  m68ki_set_fc(fc) /* auto-disable (see m68kcpu.h) */

  temp = &m68k_memory_map[((address)>>16)&0xff];
  if (temp->read8) return (*temp->read8)(ADDRESS_68K(address));
  else return READ_BYTE(temp->base, (address) & 0xffff);
}

INLINE uint m68ki_read_16_fc(uint address, uint fc)
{
  _m68k_memory_map *temp;

  m68ki_set_fc(fc) /* auto-disable (see m68kcpu.h) */
  m68ki_check_address_error(address, MODE_READ, fc) /* auto-disable (see m68kcpu.h) */
  
  temp = &m68k_memory_map[((address)>>16)&0xff];
  if (temp->read16) return (*temp->read16)(ADDRESS_68K(address));
  else return *(uint16 *)(temp->base + ((address) & 0xffff));
}

INLINE uint m68ki_read_32_fc(uint address, uint fc)
{
  _m68k_memory_map *temp;

  m68ki_set_fc(fc) /* auto-disable (see m68kcpu.h) */
  m68ki_check_address_error(address, MODE_READ, fc) /* auto-disable (see m68kcpu.h) */

  temp = &m68k_memory_map[((address)>>16)&0xff];
  if (temp->read16) return ((*temp->read16)(ADDRESS_68K(address)) << 16) | ((*temp->read16)(ADDRESS_68K(address + 2)));
  else return m68k_read_immediate_32(address);
}

INLINE void m68ki_write_8_fc(uint address, uint fc, uint value)
{
  _m68k_memory_map *temp;

  m68ki_set_fc(fc) /* auto-disable (see m68kcpu.h) */

  temp = &m68k_memory_map[((address)>>16)&0xff];
  if (temp->write8) (*temp->write8)(ADDRESS_68K(address),value);
  else WRITE_BYTE(temp->base, (address) & 0xffff, value);
}

INLINE void m68ki_write_16_fc(uint address, uint fc, uint value)
{
  _m68k_memory_map *temp;

  m68ki_set_fc(fc) /* auto-disable (see m68kcpu.h) */
  m68ki_check_address_error(address, MODE_WRITE, fc); /* auto-disable (see m68kcpu.h) */

  temp = &m68k_memory_map[((address)>>16)&0xff];
  if (temp->write16) (*temp->write16)(ADDRESS_68K(address),value);
  else *(uint16 *)(temp->base + ((address) & 0xffff)) = value;
}

INLINE void m68ki_write_32_fc(uint address, uint fc, uint value)
{
  _m68k_memory_map *temp;

  m68ki_set_fc(fc) /* auto-disable (see m68kcpu.h) */
  m68ki_check_address_error(address, MODE_WRITE, fc) /* auto-disable (see m68kcpu.h) */

  temp = &m68k_memory_map[((address)>>16)&0xff];
  if (temp->write16) (*temp->write16)(ADDRESS_68K(address),value>>16);
  else *(uint16 *)(temp->base + ((address) & 0xffff)) = value >> 16;

  temp = &m68k_memory_map[((address + 2)>>16)&0xff];
  if (temp->write16) (*temp->write16)(ADDRESS_68K(address+2),value&0xffff);
  else *(uint16 *)(temp->base + ((address + 2) & 0xffff)) = value;
}


/* --------------------- Effective Address Calculation -------------------- */

/* The program counter relative addressing modes cause operands to be
 * retrieved from program space, not data space.
 */
INLINE uint m68ki_get_ea_pcdi(void)
{
  uint old_pc = REG_PC;
  m68ki_use_program_space() /* auto-disable */
  return old_pc + MAKE_INT_16(m68ki_read_imm_16());
}


INLINE uint m68ki_get_ea_pcix(void)
{
  m68ki_use_program_space() /* auto-disable */
  return m68ki_get_ea_ix(REG_PC);
}

/* Indexed addressing modes are encoded as follows:
 *
 * Base instruction format:
 * F E D C B A 9 8 7 6 | 5 4 3 | 2 1 0
 * x x x x x x x x x x | 1 1 0 | BASE REGISTER      (An)
 *
 * Base instruction format for destination EA in move instructions:
 * F E D C | B A 9    | 8 7 6 | 5 4 3 2 1 0
 * x x x x | BASE REG | 1 1 0 | X X X X X X       (An)
 *
 * Brief extension format:
 *  F  |  E D C   |  B  |  A 9  | 8 | 7 6 5 4 3 2 1 0
 * D/A | REGISTER | W/L | SCALE | 0 |  DISPLACEMENT
 *
 * Full extension format:
 *  F     E D C      B     A 9    8   7    6    5 4       3   2 1 0
 * D/A | REGISTER | W/L | SCALE | 1 | BS | IS | BD SIZE | 0 | I/IS
 * BASE DISPLACEMENT (0, 16, 32 bit)                (bd)
 * OUTER DISPLACEMENT (0, 16, 32 bit)               (od)
 *
 * D/A:     0 = Dn, 1 = An                          (Xn)
 * W/L:     0 = W (sign extend), 1 = L              (.SIZE)
 * SCALE:   00=1, 01=2, 10=4, 11=8                  (*SCALE)
 * BS:      0=add base reg, 1=suppress base reg     (An suppressed)
 * IS:      0=add index, 1=suppress index           (Xn suppressed)
 * BD SIZE: 00=reserved, 01=NULL, 10=Word, 11=Long  (size of bd)
 *
 * IS I/IS Operation
 * 0  000  No Memory Indirect
 * 0  001  indir prex with null outer
 * 0  010  indir prex with word outer
 * 0  011  indir prex with long outer
 * 0  100  reserved
 * 0  101  indir postx with null outer
 * 0  110  indir postx with word outer
 * 0  111  indir postx with long outer
 * 1  000  no memory indirect
 * 1  001  mem indir with null outer
 * 1  010  mem indir with word outer
 * 1  011  mem indir with long outer
 * 1  100-111  reserved
 */
INLINE uint m68ki_get_ea_ix(uint An)
{
  /* An = base register */
  uint extension = m68ki_read_imm_16();

  uint Xn = 0;                        /* Index register */

  /* Calculate index */
  Xn = REG_DA[extension>>12];     /* Xn */
  if(!BIT_B(extension))           /* W/L */
    Xn = MAKE_INT_16(Xn);

  /* Add base register and displacement and return */
  return An + Xn + MAKE_INT_8(extension);
}


/* Fetch operands */
INLINE uint OPER_AY_AI_8(void)  {uint ea = EA_AY_AI_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AY_AI_16(void) {uint ea = EA_AY_AI_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AY_AI_32(void) {uint ea = EA_AY_AI_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AY_PI_8(void)  {uint ea = EA_AY_PI_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AY_PI_16(void) {uint ea = EA_AY_PI_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AY_PI_32(void) {uint ea = EA_AY_PI_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AY_PD_8(void)  {uint ea = EA_AY_PD_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AY_PD_16(void) {uint ea = EA_AY_PD_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AY_PD_32(void) {uint ea = EA_AY_PD_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AY_DI_8(void)  {uint ea = EA_AY_DI_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AY_DI_16(void) {uint ea = EA_AY_DI_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AY_DI_32(void) {uint ea = EA_AY_DI_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AY_IX_8(void)  {uint ea = EA_AY_IX_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AY_IX_16(void) {uint ea = EA_AY_IX_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AY_IX_32(void) {uint ea = EA_AY_IX_32(); return m68ki_read_32(ea);}

INLINE uint OPER_AX_AI_8(void)  {uint ea = EA_AX_AI_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AX_AI_16(void) {uint ea = EA_AX_AI_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AX_AI_32(void) {uint ea = EA_AX_AI_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AX_PI_8(void)  {uint ea = EA_AX_PI_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AX_PI_16(void) {uint ea = EA_AX_PI_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AX_PI_32(void) {uint ea = EA_AX_PI_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AX_PD_8(void)  {uint ea = EA_AX_PD_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AX_PD_16(void) {uint ea = EA_AX_PD_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AX_PD_32(void) {uint ea = EA_AX_PD_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AX_DI_8(void)  {uint ea = EA_AX_DI_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AX_DI_16(void) {uint ea = EA_AX_DI_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AX_DI_32(void) {uint ea = EA_AX_DI_32(); return m68ki_read_32(ea);}
INLINE uint OPER_AX_IX_8(void)  {uint ea = EA_AX_IX_8();  return m68ki_read_8(ea); }
INLINE uint OPER_AX_IX_16(void) {uint ea = EA_AX_IX_16(); return m68ki_read_16(ea);}
INLINE uint OPER_AX_IX_32(void) {uint ea = EA_AX_IX_32(); return m68ki_read_32(ea);}

INLINE uint OPER_A7_PI_8(void)  {uint ea = EA_A7_PI_8();  return m68ki_read_8(ea); }
INLINE uint OPER_A7_PD_8(void)  {uint ea = EA_A7_PD_8();  return m68ki_read_8(ea); }

INLINE uint OPER_AW_8(void)     {uint ea = EA_AW_8();     return m68ki_read_8(ea); }
INLINE uint OPER_AW_16(void)    {uint ea = EA_AW_16();    return m68ki_read_16(ea);}
INLINE uint OPER_AW_32(void)    {uint ea = EA_AW_32();    return m68ki_read_32(ea);}
INLINE uint OPER_AL_8(void)     {uint ea = EA_AL_8();     return m68ki_read_8(ea); }
INLINE uint OPER_AL_16(void)    {uint ea = EA_AL_16();    return m68ki_read_16(ea);}
INLINE uint OPER_AL_32(void)    {uint ea = EA_AL_32();    return m68ki_read_32(ea);}
INLINE uint OPER_PCDI_8(void)   {uint ea = EA_PCDI_8();   return m68ki_read_pcrel_8(ea); }
INLINE uint OPER_PCDI_16(void)  {uint ea = EA_PCDI_16();  return m68ki_read_pcrel_16(ea);}
INLINE uint OPER_PCDI_32(void)  {uint ea = EA_PCDI_32();  return m68ki_read_pcrel_32(ea);}
INLINE uint OPER_PCIX_8(void)   {uint ea = EA_PCIX_8();   return m68ki_read_pcrel_8(ea); }
INLINE uint OPER_PCIX_16(void)  {uint ea = EA_PCIX_16();  return m68ki_read_pcrel_16(ea);}
INLINE uint OPER_PCIX_32(void)  {uint ea = EA_PCIX_32();  return m68ki_read_pcrel_32(ea);}



/* ---------------------------- Stack Functions --------------------------- */

/* Push/pull data from the stack */
INLINE void m68ki_push_16(uint value)
{
  REG_SP = MASK_OUT_ABOVE_32(REG_SP - 2);
  m68ki_write_16(REG_SP, value);
}

INLINE void m68ki_push_32(uint value)
{
  REG_SP = MASK_OUT_ABOVE_32(REG_SP - 4);
  m68ki_write_32(REG_SP, value);
}

INLINE uint m68ki_pull_16(void)
{
  REG_SP = MASK_OUT_ABOVE_32(REG_SP + 2);
  return m68ki_read_16(REG_SP-2);
}

INLINE uint m68ki_pull_32(void)
{
  REG_SP = MASK_OUT_ABOVE_32(REG_SP + 4);
  return m68ki_read_32(REG_SP-4);
}



/* ----------------------------- Program Flow ----------------------------- */

/* Jump to a new program location or vector.
 * These functions will also call the pc_changed callback if it was enabled
 * in m68kconf.h.
 */
INLINE void m68ki_jump(uint new_pc)
{
  REG_PC = new_pc;
}

INLINE void m68ki_jump_vector(uint vector)
{
  REG_PC = m68ki_read_data_32(vector<<2);
}


/* Branch to a new memory location.
 * The 32-bit branch will call pc_changed if it was enabled in m68kconf.h.
 * So far I've found no problems with not calling pc_changed for 8 or 16
 * bit branches.
 */
INLINE void m68ki_branch_8(uint offset)
{
  REG_PC += MAKE_INT_8(offset);
}

INLINE void m68ki_branch_16(uint offset)
{
  REG_PC += MAKE_INT_16(offset);
}

INLINE void m68ki_branch_32(uint offset)
{
  REG_PC += offset;
}



/* ---------------------------- Status Register --------------------------- */

/* Set the S flag and change the active stack pointer.
 * Note that value MUST be 4 or 0.
 */
INLINE void m68ki_set_s_flag(uint value)
{
  /* Backup the old stack pointer */
  REG_SP_BASE[FLAG_S] = REG_SP;
  /* Set the S flag */
  FLAG_S = value;
  /* Set the new stack pointer */
  REG_SP = REG_SP_BASE[FLAG_S];
}


/* Set the condition code register */
INLINE void m68ki_set_ccr(uint value)
{
  FLAG_X = BIT_4(value)  << 4;
  FLAG_N = BIT_3(value)  << 4;
  FLAG_Z = !BIT_2(value);
  FLAG_V = BIT_1(value)  << 6;
  FLAG_C = BIT_0(value)  << 8;
}


/* Set the status register and check for interrupts */
INLINE void m68ki_set_sr(uint value)
{
  /* Set the status register */
  FLAG_T1 = BIT_F(value);
  FLAG_INT_MASK = value & 0x0700;
  m68ki_set_ccr(value);
  m68ki_set_s_flag((value >> 11) & 4);

  /* Check current IRQ status */
  m68ki_check_interrupts();
}


/* ------------------------- Exception Processing ------------------------- */

/* Initiate exception processing */
INLINE uint m68ki_init_exception(void)
{
  /* Save the old status register */
  uint sr = m68ki_get_sr();

  /* Turn off trace flag, clear pending traces */
  FLAG_T1 = 0;
  m68ki_clear_trace()

  /* Enter supervisor mode */
  m68ki_set_s_flag(SFLAG_SET);

  return sr;
}

/* 3 word stack frame (68000 only) */
INLINE void m68ki_stack_frame_3word(uint pc, uint sr)
{
  m68ki_push_32(pc);
  m68ki_push_16(sr);
}

#if M68K_EMULATE_ADDRESS_ERROR
/* Bus error stack frame (68000 only).
 */
INLINE void m68ki_stack_frame_buserr(uint sr)
{
  m68ki_push_32(REG_PC);
  m68ki_push_16(sr);
  m68ki_push_16(REG_IR);
  m68ki_push_32(m68ki_aerr_address);  /* access address */
  /* 0 0 0 0 0 0 0 0 0 0 0 R/W I/N FC
     * R/W  0 = write, 1 = read
     * I/N  0 = instruction, 1 = not
     * FC   3-bit function code
     */
  m68ki_push_16(m68ki_aerr_write_mode | CPU_INSTR_MODE | m68ki_aerr_fc);
}
#endif

/* Used for Group 2 exceptions.
 */
INLINE void m68ki_exception_trap(uint vector)
{
  uint sr = m68ki_init_exception();

  m68ki_stack_frame_3word(REG_PC, sr);

  m68ki_jump_vector(vector);

  /* Use up some clock cycles */
  USE_CYCLES(CYC_EXCEPTION[vector]);
}

/* Trap#n stacks a 0 frame but behaves like group2 otherwise */
INLINE void m68ki_exception_trapN(uint vector)
{
  uint sr = m68ki_init_exception();
  m68ki_stack_frame_3word(REG_PC, sr);
  m68ki_jump_vector(vector);

  /* Use up some clock cycles */
  USE_CYCLES(CYC_EXCEPTION[vector]);
}

#if M68K_EMULATE_TRACE
/* Exception for trace mode */
INLINE void m68ki_exception_trace(void)
{
  uint sr = m68ki_init_exception();

  #if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
  CPU_INSTR_MODE = INSTRUCTION_NO;
  #endif /* M68K_EMULATE_ADDRESS_ERROR */

  m68ki_stack_frame_3word(REG_PC, sr);
  m68ki_jump_vector(EXCEPTION_TRACE);

  /* Trace nullifies a STOP instruction */
  CPU_STOPPED &= ~STOP_LEVEL_STOP;

  /* Use up some clock cycles */
  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_TRACE]);
}
#endif

/* Exception for privilege violation */
static void m68ki_exception_privilege_violation(void)
{
  uint sr = m68ki_init_exception();

  #if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
  CPU_INSTR_MODE = INSTRUCTION_NO;
  #endif /* M68K_EMULATE_ADDRESS_ERROR */

  m68ki_stack_frame_3word(REG_PC-2, sr);
  m68ki_jump_vector(EXCEPTION_PRIVILEGE_VIOLATION);

  /* Use up some clock cycles and undo the instruction's cycles */
  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_PRIVILEGE_VIOLATION] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for A-Line instructions */
INLINE void m68ki_exception_1010(void)
{
  uint sr = m68ki_init_exception();
  m68ki_stack_frame_3word(REG_PC-2, sr);
  m68ki_jump_vector(EXCEPTION_1010);

  /* Use up some clock cycles and undo the instruction's cycles */
  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_1010] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for F-Line instructions */
INLINE void m68ki_exception_1111(void)
{
  uint sr = m68ki_init_exception();
  m68ki_stack_frame_3word(REG_PC-2, sr);
  m68ki_jump_vector(EXCEPTION_1111);

  /* Use up some clock cycles and undo the instruction's cycles */
  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_1111] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for illegal instructions */
INLINE void m68ki_exception_illegal(void)
{
  uint sr = m68ki_init_exception();

  #if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
  CPU_INSTR_MODE = INSTRUCTION_NO;
  #endif /* M68K_EMULATE_ADDRESS_ERROR */

  m68ki_stack_frame_3word(REG_PC-2, sr);
  m68ki_jump_vector(EXCEPTION_ILLEGAL_INSTRUCTION);

  /* Use up some clock cycles and undo the instruction's cycles */
  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_ILLEGAL_INSTRUCTION] - CYC_INSTRUCTION[REG_IR]);
}


#if M68K_EMULATE_ADDRESS_ERROR
/* Exception for address error */
INLINE void m68ki_exception_address_error(void)
{
  uint sr = m68ki_init_exception();

  /* If we were processing a bus error, address error, or reset,
     * this is a catastrophic failure.
     * Halt the CPU
     */
  if(CPU_RUN_MODE == RUN_MODE_BERR_AERR_RESET)
  {
    CPU_STOPPED = STOP_LEVEL_HALT;
    END_CYCLES(end_cycles);
    return;
  }
  CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET;

  /* Note: This is implemented for 68000 only! */
  m68ki_stack_frame_buserr(sr);

  m68ki_jump_vector(EXCEPTION_ADDRESS_ERROR);

  /* Use up some clock cycles and undo the instruction's cycles */
  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_ADDRESS_ERROR] - CYC_INSTRUCTION[REG_IR]);
}
#endif

/* Service an interrupt request and start exception processing */
INLINE void m68ki_exception_interrupt(uint int_level)
{
  uint vector, sr, new_pc;

  #if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
  CPU_INSTR_MODE = INSTRUCTION_NO;
  #endif /* M68K_EMULATE_ADDRESS_ERROR */

  /* Turn off the stopped state */
  CPU_STOPPED &= ~STOP_LEVEL_STOP;

  /* If we are halted, don't do anything */
  if(CPU_STOPPED)
    return;

  /* Always use the autovectors. */
  vector = EXCEPTION_INTERRUPT_AUTOVECTOR+int_level;

  /* Start exception processing */
  sr = m68ki_init_exception();

  /* Set the interrupt mask to the level of the one being serviced */
  FLAG_INT_MASK = int_level<<8;

  /* Acknowledge the interrupt */
  m68ki_int_ack(int_level)

  /* Get the new PC */
  new_pc = m68ki_read_data_32(vector<<2);

  /* If vector is uninitialized, call the uninitialized interrupt vector */
  if(new_pc == 0)
    new_pc = m68ki_read_data_32((EXCEPTION_UNINITIALIZED_INTERRUPT<<2));

  /* Generate a stack frame */
  m68ki_stack_frame_3word(REG_PC, sr);

  m68ki_jump(new_pc);

  /* Update cycle count now */
  USE_CYCLES(CYC_EXCEPTION[vector]);
}

/* ASG: Check for interrupts */
INLINE void m68ki_check_interrupts(void)
{
  if(CPU_INT_LEVEL > FLAG_INT_MASK)
    m68ki_exception_interrupt(CPU_INT_LEVEL>>8);
}

#endif /* _MEM68K_H_ */


/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */

#endif /* M68KCPU__HEADER */
