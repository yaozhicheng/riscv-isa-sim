#include "include/common.h"
#include "include/difftest-def.h"
#include "include/dummy_debug.h"
#include "disasm.h"

static sim_t *s = NULL;
static processor_t *p = NULL;
static state_t *state = NULL;
static disassembler_t* disassembler = NULL;

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

void sim_t::vector_unit_init()
{
  p = get_core("0");
  state = p->get_state();
}

uint64_t sim_t::vector_unit_vreg_read(reg_t vReg, reg_t n) {
    uint64_t val = p->VU.elt<uint64_t>(vReg, n);
    return val;
}

void sim_t::vector_unit_vreg_write(reg_t vReg, reg_t n, uint64_t val) {
    auto &vd = p->VU.elt<uint64_t>(vReg, n, true);
    vd = val;
}

uint64_t sim_t::vector_unit_reg_read(reg_t Reg) {
    uint64_t val = state->XPR[Reg];
    return val;
}

void sim_t::vector_unit_reg_write(reg_t Reg, uint64_t val) {
    state->XPR.write(Reg, val);
}

void sim_t::vector_unit_execute_insn(uint64_t insn) {
    p->execute_instruction(insn);
}

uint64_t sim_t::vector_unit_pc_read() {
    return state->pc;
}

void sim_t::vector_insn_print(uint64_t insn) {
    std::cout << "execute: " << disassembler->disassemble(insn) << std::endl;
}

static std::vector<std::pair<reg_t, mem_t*>> make_vector_mems(const std::vector<mem_cfg_t> &layout)
{
    std::vector<std::pair<reg_t, mem_t*>> mems;
    mems.reserve(layout.size());
    for (const auto &cfg : layout) {
        mems.push_back(std::make_pair(cfg.base, new mem_t(cfg.size)));
    }
    return mems;
}

extern "C" {

uint64_t vector_vreg_read(reg_t vReg, reg_t n) {
    return s->vector_unit_vreg_read(vReg, n);
}

void vector_vreg_write(reg_t vReg, reg_t n, uint64_t val) {
    s->vector_unit_vreg_write(vReg, n, val);
}

uint64_t vector_pc_read() {
    return s->vector_unit_pc_read();
}

uint64_t vector_reg_read(reg_t Reg) {
    return s->vector_unit_reg_read(Reg);
}

void vector_reg_write(reg_t Reg, uint64_t val) {
    s->vector_unit_reg_write(Reg, val);
}

void vector_execute_insn(uint64_t insn) {
    s->vector_unit_execute_insn(insn);
}

void vector_insn_print(uint64_t insn) {
    s->vector_insn_print(insn);
}

void vector_sim_init() {
    std::vector<mem_cfg_t> mem_cfg { mem_cfg_t(0x80000000, 0x10000000) };
    std::vector<int> hartids = {0};
    cfg_t cfg(
    // std::pair<reg_t, reg_t> default_initrd_bounds,
    std::make_pair(0, 0),
    // const char *default_bootargs,
    nullptr,
    // const char *default_isa,
    "RV64gcv",
    // const char *default_priv
    DEFAULT_PRIV,
    // const char *default_varch,
    "vlen:128,elen:64",
    // const std::vector<mem_cfg_t> &default_mem_layout,
    mem_cfg,
    // const std::vector<int> default_hartids,
    std::vector<int>{0},
    // bool default_real_time_clint
    false
  );
  std::vector<std::pair<reg_t, abstract_device_t*>> plugin_devices;
  std::vector<std::string> htif_args {"pk", "hello"};
  std::vector<std::pair<reg_t, mem_t*>> mems = make_vector_mems(cfg.mem_layout());
  s = new sim_t(
    // const cfg_t *cfg,
    &cfg,
    // bool halted,
    false,
    // std::vector<std::pair<reg_t, mem_t*>> mems
    mems,
    // std::vector<std::pair<reg_t, abstract_device_t*>> plugin_devices
    plugin_devices,
    // const std::vector<std::string>& args
    htif_args,
    // const debug_module_config_t &dm_config
    difftest_dm_config,
    // const char *log_path
    nullptr,
    //bool dtb_enabled, const char *dtb_file, FILE *cmd_file
    false, nullptr, nullptr);
    isa_parser_t isa_parser("rv64gv", DEFAULT_PRIV);
    disassembler = new disassembler_t(&isa_parser);
    s->vector_unit_init();
    p->VU.reset();
}

}
