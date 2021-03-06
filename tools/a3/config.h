#ifndef A3_CONFIG_H_
#define A3_CONFIG_H_

#define A3_VERSION "0.0.1"
#define A3_ENDPOINT "/tmp/a3_endpoint"
#define A3_1G 0x40000000ULL
#define A3_2G (A3_1G * 2)

#include "config.quadro6000.h"

#define A3_GPC_BCAST(r) (0x418000 + (r))
#define A3_BAR0_SIZE (32ULL << 20)
#define A3_BAR4_SIZE (0x1000ULL)
#define A3_GUEST_DATA_SIZE (0x1000ULL * 4)

// Because BAR3 effective area is limited to 16MB
#define A3_BAR3_TOTAL_SIZE (16 * (1ULL << 20))
#define A3_BAR3_ARENA_SIZE (A3_BAR3_TOTAL_SIZE / A3_VM_NUM)

#define A3_BAR1_TOTAL_SIZE (128ULL * (1ULL << 20))
#define A3_BAR1_POLL_AREA_SIZE (A3_CHANNELS * 0x1000ULL)  /* POLL AREA is reserved, 512KB */
#define A3_BAR1_ARENA_SIZE ((A3_BAR1_TOTAL_SIZE - A3_BAR1_POLL_AREA_SIZE) / A3_VM_NUM)

#define NOUVEAU_PV_REG_BAR 4
#define NOUVEAU_PV_SLOT_SIZE 0x1000ULL
#define NOUVEAU_PV_SLOT_NUM 64ULL
#define NOUVEAU_PV_SLOT_TOTAL (NOUVEAU_PV_SLOT_SIZE * NOUVEAU_PV_SLOT_NUM)
#define NOUVEAU_PV_BATCH_SIZE 128ULL

#define A3_PV_OPS_LIST(V)\
    V(NOUVEAU_PV_OP_SET_PGD)\
    V(NOUVEAU_PV_OP_MAP_PGT)\
    V(NOUVEAU_PV_OP_MAP)\
    V(NOUVEAU_PV_OP_MAP_BATCH)\
    V(NOUVEAU_PV_OP_MAP_SG_BATCH)\
    V(NOUVEAU_PV_OP_UNMAP_BATCH)\
    V(NOUVEAU_PV_OP_VM_FLUSH)\
    V(NOUVEAU_PV_OP_MEM_ALLOC)\
    V(NOUVEAU_PV_OP_MEM_FREE)\
    V(NOUVEAU_PV_OP_BAR3_PGT)\

#endif  // A3_CONFIG_H_
/* vim: set sw=4 ts=4 et tw=80 : */
