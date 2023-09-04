#include "include/common.h"
#include "include/difftest-def.h"
#include "include/dummy_debug.h"
#include "disasm.h"

#ifdef CONFIG_USE_SPARSEMM
#include "sparseram.h"
#endif

static debug_module_config_t difftest_dm_config = {
  .progbufsize = 2,
  .max_sba_data_width = 0,
  .require_authentication = false,
  .abstract_rti = 0,
  .support_hasel = true,
  .support_abstract_csr_access = true,
  .support_haltgroups = true,
  .support_impebreak = false
};
static csr_t_p mscratch = nullptr;

struct diff_context_t {
  word_t gpr[32];
  word_t fpr[32];
  word_t pc;
  word_t mstatus;
  word_t mcause;
  word_t mepc;
  word_t sstatus;
  word_t scause;
  word_t sepc;
  word_t satp;
  word_t mip;
  word_t mie;
  word_t mscratch;
  word_t sscratch;
  word_t mideleg;
  word_t medeleg;
  word_t mtval;
  word_t stval;
  word_t mtvec;
  word_t stvec;
  word_t priv;
  word_t debugMode;
  word_t dcsr;
  word_t dpc;
  word_t dscratch0;
  word_t dscratch1;
};

struct diff_gpr_pc_p {
  volatile const reg_t *volatile gpr;
  volatile const reg_t *pc;
};

struct diff_gpr_pc_p diff_gpr_pc = {};

static sim_t *s = NULL;
static processor_t *p = NULL;
static state_t *state = NULL;

void sim_t::diff_init(int port) {
  p = get_core("0");
  state = p->get_state();
  diff_gpr_pc.gpr = state->XPR.get_addr();
  diff_gpr_pc.pc = &(state->pc);
  mscratch = state->csrmap.find(CSR_MSCRATCH)->second;
}

void sim_t::diff_step(uint64_t n) {
  step(n);
}

void sim_t::diff_get_regs(void* diff_context) {
  struct diff_context_t *ctx = (struct diff_context_t *)diff_context;
  for (int i = 0; i < NXPR; i++) {
    ctx->gpr[i] = state->XPR[i];
  }
  for (int i = 0; i < NFPR; i++) {
    ctx->fpr[i] = unboxF64(state->FPR[i]);
  }
  ctx->pc = state->pc;
  ctx->mstatus = state->mstatus->read();
  ctx->mcause = state->mcause->read();
  ctx->mepc = state->mepc->read();
  ctx->sstatus = state->sstatus->read();
  ctx->scause = state->scause->read();
  ctx->sepc = state->sepc->read();
  ctx->satp = state->satp->read();
  ctx->mip = state->mip->read();
  ctx->mie = state->mie->read();
  ctx->mscratch = state->csrmap[CSR_MSCRATCH]->read();
  ctx->sscratch = state->csrmap[CSR_SSCRATCH]->read();
  ctx->mideleg = state->mideleg->read();
  ctx->medeleg = state->medeleg->read();
  ctx->mtval = state->mtval->read();
  ctx->stval = state->stval->read();
  ctx->mtvec = state->mtvec->read();
  ctx->stvec = state->stvec->read();
  ctx->priv = state->prv;
  ctx->debugMode = state->debug_mode;
  ctx->dcsr = state->dcsr->read();
  ctx->dpc = state->dpc->read();
  ctx->dscratch0 = state->csrmap[CSR_DSCRATCH0]->read();
  ctx->dscratch1 = state->csrmap[CSR_DSCRATCH1]->read();
}

void sim_t::diff_set_regs(void* diff_context) {
  struct diff_context_t* ctx = (struct diff_context_t*)diff_context;
  for (int i = 0; i < NXPR; i++) {
    state->XPR.write(i, (sword_t)ctx->gpr[i]);
  }
  for (int i = 0; i < NFPR; i++) {
    state->FPR.write(i, freg(f64(ctx->fpr[i])));
  }
  state->pc = ctx->pc;
  state->mstatus->write(ctx->mstatus);
  state->mcause->write(ctx->mcause);
  state->mepc->write(ctx->mepc);
  state->sstatus->write(ctx->sstatus);
  state->scause->write(ctx->scause);
  state->sepc->write(ctx->sepc);
  state->satp->write(ctx->satp);
  state->mip->write(ctx->mip);
  state->mie->write(ctx->mie);
  state->csrmap[CSR_MSCRATCH]->write(ctx->mscratch);
  state->csrmap[CSR_SSCRATCH]->write(ctx->sscratch);
  state->mideleg->write(ctx->mideleg);
  state->medeleg->write(ctx->medeleg);
  state->mtval->write(ctx->mtval);
  state->stval->write(ctx->stval);
  state->mtvec->write(ctx->mtvec);
  state->stvec->write(ctx->stvec);
  state->prv = ctx->priv;
  state->debug_mode = ctx->debugMode;
  state->dcsr->write(ctx->dcsr);
  state->dpc->write(ctx->dpc);
  state->csrmap[CSR_DSCRATCH0]->write(ctx->dscratch0);
  state->csrmap[CSR_DSCRATCH1]->write(ctx->dscratch1);
}

void sim_t::diff_memcpy(reg_t dest, void* src, size_t n) {
  mmu_t* mmu = p->get_mmu();
  #ifdef CONFIG_USE_SPARSEMM
  float dsize = 0;
  SparseRam *sp_mem = (SparseRam *)src;
  auto fc = [&](paddr_t addr, size_t len, void* buff){
    auto n = len;
    for (size_t i = 0; i < n; i++) {
      mmu->store_uint8(addr+i, *((uint8_t*)buff+i));
    }
    dsize += n;
  };
  sp_mem->copy_bytes(fc);
  printf("[sp-ram] copy data (%.2f kB) from dut complete\n", dsize/1024.0);
  #else
  for (size_t i = 0; i < n; i++) {
    mmu->store_uint8(dest+i, *((uint8_t*)src+i));
  }
  #endif
}

void sim_t::diff_debugmode(void){
  // Debug Intr causes entry to debug mode
  p->halt_request = p->HR_REGULAR;
  procs[current_proc]->step(0);
  p->halt_request = p->HR_NONE;
}

abstract_device_t * sim_t::get_debug_module(void){
  return s->plugin_devices.front().second;
}


void sim_t::diff_display() {
    int i;
  for (i = 0; i < 32; i ++) {
    printf("%4s: " FMT_WORD " ", xpr_name[i], state->XPR[i]);
    if (i % 4 == 3) {
      printf("\n");
    }
  }
  for (i = 0; i < 32; i ++) {
    printf("%4s: " FMT_WORD " ", fpr_name[i], f128_to_ui64_r_minMag(state->FPR[i], true));
    if (i % 4 == 3) {
      printf("\n");
    }
  }
  printf("pc: " FMT_WORD " mstatus: " FMT_WORD " mcause: " FMT_WORD " mepc: " FMT_WORD "\n",
      state->pc, state->mstatus->read(), state->mcause->read(), state->mepc->read());
  printf("%22s sstatus: " FMT_WORD " scause: " FMT_WORD " sepc: " FMT_WORD "\n",
      "", state->sstatus->read(), state->scause->read(), state->sepc->read());
  printf("satp: " FMT_WORD "\n", state->satp->read());
  printf("mip: " FMT_WORD " mie: " FMT_WORD " mscratch: " FMT_WORD " sscratch: " FMT_WORD "\n",
      state->mip->read(), state->mie->read(), state->csrmap[CSR_MSCRATCH]->read(), state->csrmap[CSR_MSCRATCH]->read());
  printf("mideleg: " FMT_WORD " medeleg: " FMT_WORD "\n",
      state->mideleg->read(), state->medeleg->read());
  printf("mtval: " FMT_WORD " stval: " FMT_WORD " mtvec: " FMT_WORD " stvec: " FMT_WORD "\n",
      state->mtval->read(), state->stval->read(), state->mtvec->read(), state->stvec->read());
  printf("privilege mode:%ld\n", state->prv);
  fflush(stdout);
}

extern "C" {

void difftest_memcpy(paddr_t addr, void *buf, size_t n, bool direction) {
  if (direction == DIFFTEST_TO_REF) {
    s->diff_memcpy(addr, buf, n);
  } else {
    assert(0);
  }
}

void difftest_regcpy(void* dut, bool direction) {
  if (direction == DIFFTEST_TO_REF) {
    s->diff_set_regs(dut);
  } else {
    s->diff_get_regs(dut);
  }
}

void difftest_csrcpy(void *dut, bool direction) {

}

void difftest_uarchstatus_cpy(void *dut, bool direction) {

}

void update_dynamic_config(void* config) {
#ifdef RISCV_ENABLE_COMMITLOG
  p->enable_log_commits();
#endif
}

void difftest_exec(uint64_t n) {
  s->diff_step(n);
}

void difftest_init(int port) {
  std::vector<std::string> difftest_htif_args;
  difftest_htif_args.push_back("");
  char mem_layout_str[100];
  sprintf(mem_layout_str, "0x%x:0x%lx", DRAM_BASE, CONFIG_MSIZE);
  auto memory_layout = parse_mem_layout(mem_layout_str);
  static std::vector<std::pair<reg_t, abstract_device_t*>> difftest_plugin_devices{
  std::make_pair(reg_t(DM_BASE_ADDR), new dummy_debug_t)};
  auto const cfg = new cfg_t(
    // std::pair<reg_t, reg_t> default_initrd_bounds,
    std::make_pair(0, 0),
    // const char *default_bootargs,
    nullptr,
    // const char *default_isa,
    "RV64IMAFDC_zba_zbb_zbc_zbs_zbkb_zbkc_zbkx_zknd_zkne_zknh_zksed_zksh_svinval",
    // const char *default_priv
    DEFAULT_PRIV,
    // const char *default_varch,
    DEFAULT_VARCH,
    // const std::vector<mem_cfg_t> &default_mem_layout,
    memory_layout,
    // const std::vector<int> default_hartids,
    std::vector<int>{0},
    // bool default_real_time_clint
    false
  );
  s = new sim_t(
    // const cfg_t *cfg,
    cfg,
    // bool halted,
    false,
    // std::vector<std::pair<reg_t, mem_t*>> mems
    make_mems(memory_layout),
    // std::vector<std::pair<reg_t, abstract_device_t*>> plugin_devices
    difftest_plugin_devices,
    // const std::vector<std::string>& args
    difftest_htif_args,
    // const debug_module_config_t &dm_config
    difftest_dm_config,
    // const char *log_path
    DIFFTEST_LOG_FILE,
    //bool dtb_enabled, const char *dtb_file, FILE *cmd_file
    false, nullptr, nullptr);
  s->diff_init(port);
#ifdef RISCV_ENABLE_COMMITLOG
  setvbuf(p->get_log_file(), NULL, _IONBF, 0);
#endif
}

void difftest_raise_intr(uint64_t NO) {
  if (NO == 0xc) {
    s->diff_debugmode();  // Debug Intr
  } else {
    uint64_t mip_bit = 0x1UL << (NO & 0xf);
    bool is_timer_interrupt = mip_bit & 0xa0UL;
    bool is_external_interrupt = mip_bit & 0xb00UL;
    bool from_outside = !(mip_bit & state->mip->read());
    bool external_set = (is_timer_interrupt || is_external_interrupt) && from_outside;
    if (external_set) {
      state->mip->backdoor_write_with_mask(mip_bit, mip_bit);
      difftest_exec(1);
      state->mip->backdoor_write_with_mask(mip_bit, ~mip_bit);
    } else {
      difftest_exec(1);
    }
  }
}

void isa_reg_display() {
  s->diff_display();
}

int difftest_store_commit(uint64_t *addr, uint64_t *data, uint8_t *mask) {
  return 0;
}

uint64_t difftest_guided_exec(void *) {
  difftest_exec(1);
  return 0;
}

void debug_mem_sync(reg_t addr, void* buf, size_t n) {
  // addr is absolute physical addr
  // now we are not using this right now because I don't know how spike does if from device
  dummy_debug_t* dummy_debug = (dummy_debug_t *) s->get_debug_module();
  // start addr is virtual addr used by simulator
  int offset = (addr - DM_BASE_ADDR) / sizeof(uint8_t);
  uint8_t* start_addr = dummy_debug->dummy_debug_mem + offset;
  memcpy(start_addr, buf, n); // TODO copy to device not addr
}

void difftest_load_flash(void *flash_bin, size_t size) {

}

}
