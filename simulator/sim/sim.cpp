#include "verilated_dpi.h"
#include "VCPU.h"
#include "verilated_vcd_c.h"
#include <bits/stdc++.h>
#include "include/debug.h"
#include "include/common.h"
#include "include/disasm.h"
#include "memory/paddr.h"

using namespace std;

extern VCPU *dut;
extern uint64_t sim_time;
extern VerilatedVcdC *m_trace;
extern uint8_t pmem[];

typedef struct {
  word_t pc;
  word_t inst;
  char asm_buf[128];
} inst_log;

void print_itrace(inst_log *inst_log, VCPU *dut);
void difftest_step();

void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);


static const char *csr_names[] = {"mstatus", "mtvec", "mepc",  "mcause"};
// uint32_t *cpu_mstatus = NULL, *cpu_mtvec = NULL, *cpu_mepc = NULL, *cpu_mcause = NULL;

// load the state of your simulated cpu into sim_cpu
// called in every sim cycle
void set_state() {
  sim_cpu.pc = dut->pc_cur;
  memcpy(&sim_cpu.gpr[0], cpu_gpr, 4 * 32);
  sim_cpu.csr.mstatus = *cpu_csr[CSR_MSTATUS];
  sim_cpu.csr.mtvec   = *cpu_csr[CSR_MTVEC];
  sim_cpu.csr.mepc    = *cpu_csr[CSR_MEPC];
  sim_cpu.csr.mcause  = *cpu_csr[CSR_MCAUSE];
}

// num of executed instruction
uint64_t g_nr_guest_inst = 0;

// simulate a single cycle
void single_cycle() {
  dut->clk = 1;
  dut->eval();
  m_trace->dump(sim_time++); 
  dut->clk = 0;
  #ifdef AXI
  pmem_write();
  pmem_read();
  #endif
  dut->eval();
  m_trace->dump(sim_time++); 
  if(dut->commit_wb == 1) set_state();
}

// simulate a reset
void reset(int n) {
  dut->clk = 0;
  dut->rstn = 0;
  dut->eval();
  while (n-- > 0) {
    single_cycle();
  }
  dut->rstn = 1;
  dut->eval();
}

// ** End simulation by software **
// check if the program should end
inline bool test_break(){
  return dut->inst == 0x00100073U;
}

static void statistic() {
  Log("total guest instructions = %ld", g_nr_guest_inst);
}

void device_update();
// init the running state of our simulator
SimState sim_state = { .state = SIM_STOP };

// execute n instructions
void cpu_exec(unsigned int n){
  switch (sim_state.state) {
    case SIM_END: case SIM_ABORT: case SIM_QUIT:
      printf("Program execution has ended. To restart the program, exit NPC and run again.\n");
      return;
    default: sim_state.state = SIM_RUNNING;
  }
  inst_log inst_log;
  bool npc_cpu_uncache_pre = 0;
  while (n--) {
    #ifdef CONFIG_ITRACE
    if(n < CONFIG_ITRACE_MAX_INST){
      print_itrace(&inst_log, dut);
    }
    #endif
    
    // execute single instruction
    if(test_break()) {
      // set the end state
      sim_state.halt_pc = dut->pc_cur;
      sim_state.halt_ret = cpu_gpr[10];
      sim_state.state = SIM_END;
      break;
    }

    if (dut->commit_wb) {
      if(npc_cpu_uncache_pre){
        difftest_sync();
      }

      difftest_step();
  
      g_nr_guest_inst++;
      npc_cpu_uncache_pre = dut->uncache_read_wb;
    }
    // your cpu step a cycle
    single_cycle();

#ifdef DEVICE
    device_update();
#endif
    if(sim_state.state != SIM_RUNNING) break;
  }

  switch (sim_state.state) {
    case SIM_RUNNING: sim_state.state = SIM_STOP; break;
    case SIM_END: case SIM_ABORT:
      Log("sim: %s at pc = " FMT_WORD,
          (sim_state.state == SIM_ABORT ? ANSI_FMT("ABORT", ANSI_FG_RED) :
           (sim_state.halt_ret == 0 ? ANSI_FMT("HIT GOOD TRAP", ANSI_FG_GREEN) :
            ANSI_FMT("HIT BAD TRAP", ANSI_FG_RED))),
          sim_state.halt_pc);
      // fall through
    case SIM_QUIT: statistic();
  }
}

static const char *regs[] = {
  "$0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

// map the name of reg to its value
word_t isa_reg_str2val(const char *s, bool *success) {;
  if(!strcmp(s, "pc")){
    *success = true;
    return dut->pc_cur;
  }
  for(int i = 0; i < 32; i++){
    if(!strcmp(s, regs[i])){
      *success = true;
      return cpu_gpr[i];
    }
  }
  *success = false;
  return 0;
}

/********** RTL Registers' Interface **********/
// set cpu_gpr point to your cpu's gpr
extern "C" void set_gpr_ptr(const svOpenArrayHandle r) {
  cpu_gpr = (uint32_t *)(((VerilatedDpiOpenVar*)r)->datap());
}
// set the pointers pint to you cpu's csr
extern "C" void set_csr_ptr(const svOpenArrayHandle mstatus, const svOpenArrayHandle mtvec, const svOpenArrayHandle mepc, const svOpenArrayHandle mcause) {
  cpu_csr[CSR_MSTATUS] = (uint32_t *)(((VerilatedDpiOpenVar*)mstatus)->datap());
  cpu_csr[CSR_MTVEC] = (uint32_t *)(((VerilatedDpiOpenVar*)mtvec)->datap());
  cpu_csr[CSR_MEPC] = (uint32_t *)(((VerilatedDpiOpenVar*)mepc)->datap());
  cpu_csr[CSR_MCAUSE] = (uint32_t *)(((VerilatedDpiOpenVar*)mcause)->datap());
}
/**********************************************/

void isa_reg_display() {
  for (int i = 0; i < 32; i++) {
    printf("gpr[%d](%s) = 0x%x\n", i, regs[i], cpu_gpr[i]);
  }
  for (int i = 0; i < NR_CSR; i++) {
    printf("csr(%s) = 0x%08x\n", csr_names[i], *cpu_csr[i]);
  }
}

void print_itrace(inst_log *inst_log, VCPU *dut) {

  inst_log->pc = dut->pc_cur;
  inst_log->inst = dut->inst;
  disassemble(inst_log->asm_buf, sizeof(inst_log->asm_buf), inst_log->pc, (uint8_t *)&inst_log->inst, 4);
  printf("0x%08x: %08x\t%s\n", inst_log->pc, inst_log->inst, inst_log->asm_buf);
}