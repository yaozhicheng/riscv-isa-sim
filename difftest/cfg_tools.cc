#include "include/common.h"
#include "mmu.h"

/**
 * These functions are brought from spike_main/spike.cc.
 */

static bool sort_mem_region(const mem_cfg_t &a, const mem_cfg_t &b)
{
  if (a.base == b.base)
    return (a.size < b.size);
  else
    return (a.base < b.base);
}

static void merge_overlapping_memory_regions(std::vector<mem_cfg_t> &mems)
{
  // check the user specified memory regions and merge the overlapping or
  // eliminate the containing parts
  assert(!mems.empty());

  std::sort(mems.begin(), mems.end(), sort_mem_region);
  for (auto it = mems.begin() + 1; it != mems.end(); ) {
    reg_t start = prev(it)->base;
    reg_t end = prev(it)->base + prev(it)->size;
    reg_t start2 = it->base;
    reg_t end2 = it->base + it->size;

    //contains -> remove
    if (start2 >= start && end2 <= end) {
      it = mems.erase(it);
    //partial overlapped -> extend
    } else if (start2 >= start && start2 < end) {
      prev(it)->size = std::max(end, end2) - start;
      it = mems.erase(it);
    // no overlapping -> keep it
    } else {
      it++;
    }
  }
}

std::vector<mem_cfg_t> parse_mem_layout(const char* arg)
{
  std::vector<mem_cfg_t> res;

  // handle legacy mem argument
  char* p;
  auto mb = strtoull(arg, &p, 0);
  if (*p == 0) {
    reg_t size = reg_t(mb) << 20;
    if (size != (size_t)size)
      throw std::runtime_error("Size would overflow size_t");
    res.push_back(mem_cfg_t(reg_t(DRAM_BASE), size));
    return res;
  }

  // handle base/size tuples
  while (true) {
    auto base = strtoull(arg, &p, 0);
    auto size = strtoull(p + 1, &p, 0);

    // page-align base and size
    auto base0 = base, size0 = size;
    size += base0 % PGSIZE;
    base -= base0 % PGSIZE;
    if (size % PGSIZE != 0)
      size += PGSIZE - size % PGSIZE;

    if (size != size0) {
      fprintf(stderr, "Warning: the memory at  [0x%llX, 0x%llX] has been realigned\n"
                      "to the %ld KiB page size: [0x%llX, 0x%llX]\n",
              base0, base0 + size0 - 1, long(PGSIZE / 1024), base, base + size - 1);
    }

    res.push_back(mem_cfg_t(reg_t(base), reg_t(size)));
    if (!*p)
      break;
    arg = p + 1;
  }

  merge_overlapping_memory_regions(res);

  return res;
}

std::vector<std::pair<reg_t, mem_t*>> make_mems(const std::vector<mem_cfg_t> &layout)
{
  std::vector<std::pair<reg_t, mem_t*>> mems;
  mems.reserve(layout.size());
  for (const auto &cfg : layout) {
    mems.push_back(std::make_pair(cfg.base, new mem_t(cfg.size)));
  }
  return mems;
}
