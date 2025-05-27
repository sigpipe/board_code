// h_vhdl_extract.h
// hardware access constants
// This file was automatically generated
// by Register Extractor (ver 4.13) on Tue May 27 20:45:19 2025
// compile version Tue Jun 29 16:33:14 2021
// current dir:  C:\reilly\proj\quanet\quanet_hdl\projects\daq3\zcu106
// DO NOT MODIFY THIS FILE!

// source files:
//    ../../scripts/..\..\..\quanet_hdl\library\quanet\global_pkg.vhd
//    ../../scripts/..\..\..\quanet_hdl\library\quanet_dac\quanet_dac.vhd
//    ../../scripts/..\..\..\quanet_hdl\library\quanet_adc\quanet_adc.vhd

#ifndef __H_VHDL_EXTRACT_H__
#define __H_VHDL_EXTRACT_H__


// version constants
#define H_VHDL_EXTRACT_VER (4)
#define H_VHDL_EXTRACT_SUBVER (13)
#define H_VHDL_EXTRACT_DATE "Tue May 27 20:45:19 2025"
#define H_VHDL_EXTRACT_DIR "C:\reilly\proj\quanet\quanet_hdl\projects\daq3\zcu106"


// all extracted constants
          // -- std_logic_vector(31 downto 0) := x"8fffffff");
          // -- std_logic_vector(31 downto 0) := x"80000000";  -- start addr in DDR
          // -- std_logic_vector(G_BODY_CHAR_POLY)'length;
#define H_CORR_MEM_D_W (16)
#define H_CTR_W (4)
#define H_PASS_W (6)
#define H_CORR_MAG_W (10)
#define H_MAX_SLICES (4)
#define H_BODY_LEN_W (10)
#define H_BODY_RAND_BITS (4)
#define H_BODY_CHAR_POLY (0x080001)
#define H_FRAME_QTY_W (16)
#define H_OSAMP_W (2)
#define H_HDR_LEN_W (8)
#define H_FRAME_PD_W (24)

// regextractor can handle more than one register space
#define H_NUM_REGSPACES (2)

// inferred register locations and fields
//   opt_old_consts 0

//
// register space DAC
//

#define H_DAC (0)

#define H_DAC_FR1                     0x00000000  /* 0 */
#define H_DAC_FR1_FRAME_PD_MIN1       0x00000300  /* 0x00ffffff  rw */
                                       // r 0xffffffff
                                       // w 0xffffffff

#define H_DAC_FR2                     0x00001000  /* 1 */
#define H_DAC_FR2_FRAME_QTY_MIN1      0x00001200  /* 0x0000ffff  rw */
                                       // r 0xffffffff
                                       // w 0xffffffff

#define H_DAC_CTL                     0x00002000  /* 2 */
#define H_DAC_CTL_REG_W               0x00002400  /* 0xffffffff  r  */
#define H_DAC_CTL_BODY_LEN_MIN1       0x00002140  /* 0x000003ff   w */
#define H_DAC_CTL_OSAMP_MIN1          0x0000204a  /* 0x00000c00   w -- oversampling: 0=1,1=2,3=4 */
#define H_DAC_CTL_HDR_LEN_MIN1        0x0000210c  /* 0x000ff000   w -- in cycles, minus 1 */
#define H_DAC_CTL_IM_PREEMPH          0x00002036  /* 0x00400000   w -- preemphasis for IM */
#define H_DAC_CTL_TX_DBITS            0x00002037  /* 0x00800000   w -- alice transmits data */
#define H_DAC_CTL_SAME_HDRS           0x00002038  /* 0x01000000   w -- tx all the same hdr */
#define H_DAC_CTL_ALICE_SYNCING       0x00002039  /* 0x02000000   w -- means I am alice, doing sync */
#define H_DAC_CTL_MEMTX_CIRC          0x0000203a  /* 0x04000000   w -- circular xmit from mem */
#define H_DAC_CTL_TX_0                0x0000203b  /* 0x08000000   w -- header contains zeros */
#define H_DAC_CTL_TX_ALWAYS           0x0000203c  /* 0x10000000   w -- used for dbg to view on scope */
#define H_DAC_CTL_USE_LFSR            0x0000203d  /* 0x20000000   w -- header contains lfsr */
#define H_DAC_CTL_RAND_BODY           0x0000203e  /* 0x40000000   w -- bob sets to scramble frame bodies */
#define H_DAC_CTL_TX_UNSYNC           0x0000203f  /* 0x80000000   w -- PROBALY WILL GO AWAY */
                                       // r 0xffffffff
                                       // w 0xffcfffff

#define H_DAC_STATUS                  0x00003000  /* 3 */
#define H_DAC_STATUS_GTH_STATUS       0x00003080  /* 0x0000000f  r  */
#define H_DAC_STATUS_VERSION          0x00003084  /* 0x000000f0  r  -- assign reg3_r[31:8] = 24'h0; */
                                       // r 0x000000ff
                                       // w 0x00000000

#define H_DAC_IM                      0x00004000  /* 4 */
#define H_DAC_IM_BODY                 0x00004200  /* 0x0000ffff   w */
#define H_DAC_IM_REG_W                0x00004400  /* 0xffffffff  r  */
#define H_DAC_IM_HDR                  0x00004210  /* 0xffff0000   w */
                                       // r 0xffffffff
                                       // w 0xffffffff

#define H_DAC_HDR                     0x00005000  /* 5 */
#define H_DAC_HDR_LFSR_RST_ST         0x00005160  /* 0x000007ff   w -- often 11'b10100001111 */
#define H_DAC_HDR_REG_W               0x00005400  /* 0xffffffff  r  */
#define H_DAC_HDR_SYMS_PER_FRAME_MIN1 0x000050cb  /* 0x0001f800   w */
#define H_DAC_HDR_DBIT_CYCS_MIN1      0x00005091  /* 0x001e0000   w */
                                       // r 0xffffffff
                                       // w 0x001fffff

#define H_DAC_DMA                     0x00006000  /* 6 */
#define H_DAC_DMA_REG_W               0x00006400  /* 0xffffffff  r  */
                                       // r 0xffffffff
                                       // w 0x00000000

//
// register space ADC
//

#define H_ADC (1)

#define H_ADC_ACTL                 0x10000000  /* 0 */
#define H_ADC_ACTL_AREG_W          0x10000400  /* 0xffffffff  r  */
#define H_ADC_ACTL_MEAS_NOISE      0x10000021  /* 0x00000002   w */
#define H_ADC_ACTL_TXRX_EN         0x10000022  /* 0x00000004   w */
#define H_ADC_ACTL_OSAMP_MIN1      0x10000044  /* 0x00000030   w */
#define H_ADC_ACTL_SEARCH          0x10000026  /* 0x00000040   w */
#define H_ADC_ACTL_CORRSTART       0x10000027  /* 0x00000080   w */
#define H_ADC_ACTL_ALICE_SYNCING   0x10000029  /* 0x00000200   w */
#define H_ADC_ACTL_LFSR_RST_ST     0x10000170  /* 0x07ff0000   w */
                                       // r 0xffffffff
                                       // w 0x07ff02f6

#define H_ADC_STAT                 0x10001000  /* 1 */
#define H_ADC_STAT_DMA_XFER_REQ_RC 0x10001020  /* 0x00000001  r  -- for dbg */
#define H_ADC_STAT_DMA_WREADY_CNT  0x10001084  /* 0x000000f0  r  */
#define H_ADC_STAT_ADC_GO_CNT      0x10001088  /* 0x00000f00  r  */
#define H_ADC_STAT_XFER_REQ_CNT    0x1000108c  /* 0x0000f000  r  */
#define H_ADC_STAT_ADCFIFO_VER     0x10001118  /* 0xff000000  r  */
                                       // r 0xff00fff1
                                       // w 0x00000000

#define H_ADC_CSTAT                0x10002000  /* 2 */
#define H_ADC_CSTAT_PROC_SEL       0x10002060  /* 0x00000007   w */
#define H_ADC_CSTAT_PROC_DOUT      0x10002400  /* 0xffffffff  r  */
                                       // r 0xffffffff
                                       // w 0x00000007

#define H_ADC_FR1                  0x10003000  /* 3 */
#define H_ADC_FR1_AREG_W           0x10003400  /* 0xffffffff  r  */
#define H_ADC_FR1_FRAME_PD_MIN1    0x10003300  /* 0x00ffffff   w */
#define H_ADC_FR1_NUM_PASS_MIN1    0x100030d8  /* 0x3f000000   w */
                                       // r 0xffffffff
                                       // w 0x3fffffff

#define H_ADC_FR2                  0x10004000  /* 4 */
#define H_ADC_FR2_AREG_W           0x10004400  /* 0xffffffff  r  */
#define H_ADC_FR2_HDR_LEN_MIN1     0x10004100  /* 0x000000ff   w */
#define H_ADC_FR2_FRAME_QTY_MIN1   0x10004210  /* 0xffff0000   w */
                                       // r 0xffffffff
                                       // w 0xffff00ff

#define H_ADC_HDR                  0x10005000  /* 5 */
#define H_ADC_HDR_AREG_W           0x10005400  /* 0xffffffff  r  */
#define H_ADC_HDR_PWR_THRESH       0x100051c0  /* 0x00003fff   w */
#define H_ADC_HDR_THRESH           0x1000514e  /* 0x00ffc000   w */
                                       // r 0xffffffff
                                       // w 0x00ffffff

#define H_ADC_SYNC                 0x10006000  /* 6 */
#define H_ADC_SYNC_AREG_W          0x10006400  /* 0xffffffff  r  */
#define H_ADC_SYNC_DLY             0x10006300  /* 0x00ffffff   w */
                                       // r 0xffffffff
                                       // w 0x00ffffff

#define H_ADC_PCTL                 0x10007000  /* 7 */
#define H_ADC_PCTL_CLR_CTRS        0x10007020  /* 0x00000001  rw */
#define H_ADC_PCTL_PROC_CLR_CNTS   0x10007028  /* 0x00000100  rw */
                                       // r 0xffffffff
                                       // w 0xffffffff
// initializer for h_baseaddr array
#define H_BASEADDR_INIT { 0x44a00000  /* for DAC */, \
		0xaaa00000  /* for ADC */}


// bit field manipulation routines
// (these don't read or write hardware)
int  h_ext(int fldconst, int regval);
int  h_ins(int fldconst, int regval, int v);

// hardware register read and write routines
int  h_r(int regconst);
void h_w(int regconst, int v);

// routines that read and write fields in registers
int  h_r_fld(int fldconst);
void h_rw_fld(int fldconst, int v);
void h_w_fld(int fldconst, int v);
void h_set_fld(int fldconst);
void h_clr_fld(int fldconst);
void h_pulse_fld(int fldconst);

// register constants:  
//   space 31:28
//   reg   27:12
// field constants:
//   space 31:28
//   reg   27:12
//   bwid  9:5
//   bpos  4:0

// register & field constant conversion & macros
#define H_CONST(s,r,w,p)  (((s&0xf)<<28) | ((r&0xffff)<<12) | ((w&0x3f)<<5) | (p&0x1f))
#define H_2SPACE(c)    (c>>28 & 0xf)
#define H_2REG(c)      (c>>12 & 0xffff)
#define H_2BWID(c)     (c>>5 & 0x3f)
#define H_2BPOS(c)     (c>>0 & 0x1f)
#define H_2VMASK(c)    ((H_2BWID(c)==32)?~0:~(~0<<H_2BWID(c)))
#define H_2MASK(c)     (H_2VMASK(c)<<H_2BPOS(c))
// the following are to access offsets from reg addr or bit position
#define H_2REGOFF(c,i)  ((c&~0xffff000) | (((H_2REG(c)+i)&0xffff)<<12))
#define H_2BPOSOFF(c,i) ((c&~0x1f) | ((H_2BPOS(c)+i)&0x1f))


extern int h_baseaddr[];

// To reduce the size of your compiled program
// use the function calls (lowercase) instead of these macros:
// To make your program faster, use these macros,
// but any speedup will probably be insignificant!
#define H_EXT(c, rv)    ((rv >> H_2BPOS(c)) & H_2VMASK(c))
#define H_INS(c, rv, v) ((rv & ~H_2MASK(c)) | ((v & H_2VMASK(c)) << H_2BPOS(c)))
// Note that H_W and H_R can never call custom register access functions
#define H_R(c)      XIo_In32(h_baseaddr[H_2SPACE(c)] + H_2REG(c)*4)
#define H_W(c, rv)  XIo_Out32(h_baseaddr[H_2SPACE(c)] + H_2REG(c)*4, rv)
#define H_R_FLD(c)     H_EXT(c, H_R(c))
#define H_RW_FLD(c, v) H_W(c, H_INS(c, H_R(c), v))
#define H_W_FLD(c, v)  H_W(c, H_INS(c, 0,      v))

typedef int  h_r_t(int regconst);
typedef void h_w_t(int regconst, int v);
typedef struct h_access_funcs_st {
  h_r_t *r;
  h_w_t *w;
} h_access_funcs_t;

// If your registers are to be accessed using simple XIo_Out32 & XIo_In32
// calls, you don't need either of these initialization routines.
// But if you want to write your own custom register access routines for
// a particular register space, make the routines conform to h_w_t and/or
// h_r_t.  Then hook them in by passing pointers to these routines to
// the following:
void h_set_access_funcs(int rs, h_r_t *r, h_w_t *w);

#endif
