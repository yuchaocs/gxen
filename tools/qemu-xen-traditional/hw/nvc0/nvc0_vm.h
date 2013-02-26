#ifndef HW_NVC0_NVC0_VM_H_
#define HW_NVC0_NVC0_VM_H_
#include "nvc0.h"
namespace nvc0 {

uint32_t vm_bar1_read(nvc0_state_t* state, target_phys_addr_t offset);
void vm_bar1_write(nvc0_state_t* state, target_phys_addr_t offset, uint32_t value);
uint32_t vm_bar3_read(nvc0_state_t* state, target_phys_addr_t offset);
void vm_bar3_write(nvc0_state_t* state, target_phys_addr_t offset, uint32_t value);

}  // namespace nvc0
#endif  // HW_NVC0_NVC0_VM_H_
/* vim: set sw=4 ts=4 et tw=80 : */