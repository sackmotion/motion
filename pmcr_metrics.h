/*
 * pmcr_metrics.h
 *
 *  Created on: 24 Jun 2013
 *      Author: ntuckett
 */

#ifndef PMCR_METRICS_H_
#define PMCR_METRICS_H_

#if defined(ARM_PMCR_METRICS)

#define SCP_CR_CYCLE_COUNT          0xff
#define SCP_CR_RETURN_PRED_WRONG    0x26
#define SCP_CR_RETURN_PRED_RIGHT    0x25
#define SCP_CR_RETURN_COUNT         0x24
#define SCP_CR_CALL_COUNT           0x23
#define SCP_CR_WRITE_BUFFER_DRAIN   0x12
#define SCP_CR_LOAD_STALL           0x11
#define SCP_CR_EXTERNAL_DATA_ACCESS 0x10
#define SCP_CR_MAIN_TLB_MISS        0x0F
#define SCP_CR_PC_SOFT_CHANGE       0x0D
#define SCP_CR_DATA_CACHE_WRITEBACK 0x0C
#define SCP_CR_DATA_CACHE_MISS      0x0B
#define SCP_CR_DATA_CACHE_ACCESS    0x0A
#define SCP_CR_DATA_CACHE_ACCESS_C  0x09        // for only cacheable locations
#define SCP_CR_INSTRUCTION_COUNT    0x07
#define SCP_CR_BRANCH_MISPREDICTION 0x06
#define SCP_CR_BRANCH_COUNT         0x05
#define SCP_CR_DATA_MICRO_TLB_MISS  0x04
#define SCP_CR_INSTR_MICRO_TLB_MISS 0x03
#define SCP_CR_DATA_STALL           0x02
#define SCP_CR_INSTRUCTION_STALL    0x01
#define SCP_CR_INSTR_CACHE_MISS     0x00

__attribute__((always_inline)) inline void scp_pmcr_reset_and_start_counters()
{
    asm volatile ("mcr p15,  0, %0, c15,  c12, 0\n" : : "r" (7));
}

__attribute__((always_inline)) inline void scp_pmcr_configure_reset_and_start_counters(unsigned int cr0, unsigned int cr1)
{
    asm volatile ("mcr p15,  0, %0, c15,  c12, 0\n" : : "r" (7 | (cr0 << 12) | (cr1 << 20)));
}

__attribute__((always_inline)) inline void scp_pmcr_stop_counters()
{
    asm volatile ("mcr p15,  0, %0, c15,  c12, 0\n" : : "r" (0));
}

__attribute__((always_inline)) inline unsigned int scp_ccr_read(void)
{
  unsigned int cc;
  asm volatile ("mrc p15, 0, %0, c15, c12, 1" : "=r" (cc));
  return cc;
}

__attribute__((always_inline)) inline unsigned int scp_cr0_read(void)
{
  unsigned int cr;
  asm volatile ("mrc p15, 0, %0, c15, c12, 2" : "=r" (cr));
  return cr;
}

__attribute__((always_inline)) inline unsigned int scp_cr1_read(void)
{
  unsigned int cr;
  asm volatile ("mrc p15, 0, %0, c15, c12, 3" : "=r" (cr));
  return cr;
}

#endif /* ARM_PMCR_METRICS */
#endif /* PMCR_METRICS_H_ */
