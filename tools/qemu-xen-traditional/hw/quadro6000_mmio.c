/*
 * NVIDIA Quadro6000 MMIO model
 *
 * Copyright (c) 2012-2013 Yusuke Suzuki
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "quadro6000.h"
#include "quadro6000_vbios.inc"

// crystal freq is 27000KHz
#define GPU_CLOCKS_PER_NANO_SEC 27
#define GPU_CLOCKS_PER_SEC (GPU_CLOCKS_PER_NANO_SEC * 1000 * 1000)
static uint32_t timer_numerator = 0;
static uint32_t timer_denominator = 0;
static uint64_t timer_nano_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000000ULL) + ts.tv_nsec;
}
static uint64_t timer_now(void) {
    const uint64_t nano = timer_nano_sec();
    return nano * GPU_CLOCKS_PER_NANO_SEC * (timer_numerator + 1) / (timer_denominator + 1);
}

// wrappers
static inline uint8_t read8(void* ptr, ptrdiff_t offset) {
    return quadro6000_read8(((uint8_t*)ptr) + offset);
}

static inline uint16_t read16(void* ptr, ptrdiff_t offset) {
    return quadro6000_read16(((uint8_t*)ptr) + offset);
}

static inline uint32_t read32(void* ptr, ptrdiff_t offset) {
    return quadro6000_read32(((uint8_t*)ptr) + offset);
}

static inline void write8(void* ptr, ptrdiff_t offset, uint8_t data) {
    quadro6000_write8(data, ((uint8_t*)ptr) + offset);
}

static inline void write16(void* ptr, ptrdiff_t offset, uint16_t data) {
    quadro6000_write16(data, ((uint8_t*)ptr) + offset);
}

static inline void write32(void* ptr, ptrdiff_t offset, uint32_t data) {
    quadro6000_write32(data, ((uint8_t*)ptr) + offset);
}

// http://nouveau.freedesktop.org/wiki/HwIntroduction
// BAR 0:
//   control registers. 16MB in size. Is divided into several areas for
//   each of the functional blocks of the card.
static void quadro6000_init_bar0(quadro6000_state_t* state) {
    void* ptr = qemu_mallocz(0x2000000);
    state->bar[0].space = ptr;
    write32(ptr, NV03_PMC_BOOT_0, QUADRO6000_REG0);

    // map vbios
    Q6_PRINTF("BIOS size ... %lu\n", sizeof(quadro6000_vbios));
    memcpy(state->bar[0].space + NV_PROM_OFFSET, quadro6000_vbios, sizeof(quadro6000_vbios));

    // and initialization information from BIOS
    #include "quadro6000_init.inc"
}

static uint32_t quadro6000_mmio_bar0_readb(void *opaque, target_phys_addr_t addr) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[0].addr;
    // return read8(state->bar[0].space, offset);
    return read8(state->bar[0].real, offset);
}

static uint32_t quadro6000_mmio_bar0_readw(void *opaque, target_phys_addr_t addr) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[0].addr;
    // return read16(state->bar[0].space, offset);
    return read16(state->bar[0].real, offset);
}

static void quadro6000_mmio_bar0_writeb(void *opaque, target_phys_addr_t addr, uint32_t val) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[0].addr;
    // write8(state->bar[0].space, offset, val);
    write8(state->bar[0].real, offset, val);
}

static void quadro6000_mmio_bar0_writew(void *opaque, target_phys_addr_t addr, uint32_t val) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[0].addr;
    // write16(state->bar[0].space, offset, val);
    write16(state->bar[0].real, offset, val);
}

static uint32_t quadro6000_mmio_bar0_readd(void *opaque, target_phys_addr_t addr) {
    // Q6_PRINTF("MMIO bar0 readd 0x%X\n", addr);
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[0].addr;
    switch (offset) {
    case NV50_PMC_BOOT_0:  // 0x00000000
        // return QUADRO6000_REG0;
        // for debugging, we access to real device
        return read32(state->bar[0].real, offset);

    // see nvc0_vram.c
    // Part struct starts with 0x11020c (PBFBs)
    case 0x022438:  // parts
        // Quadro6000 has 6 parts
        // return 0x00000006;
        // Q6_PRINTF("MMIO bar0 parts 0x%X : 0x%X\n", addr, 1);
        return 0x00000001;  // Expose only 1 parts by Quadro6000 device model

    case 0x022554:  // pmask
        // Q6_PRINTF("MMIO bar0 pmask 0x%X : 0x%X\n", addr, 0);
        return 0x00000000;

    case 0x10f20c:  // bsize
        // Quadro6000 has 0x00000400 size vram per part
        // Actually, because of << 20, VRAM part size is 1GB
        // Q6_PRINTF("MMIO bar0 bsize 0x%X : 0x%X\n", addr, 0x400);
        return 0x00000400;

    case 0x100800:  //  ?
        break;
        // return 0x00000006;

    case (0x11020c + 0 * 0x1000):  // part 0 size
        return 0x00000400;

    case 0x409604:
        // gpc_nr TODO(Yusuke Suzuki) fix upper bits
        // gpc_nr(0x4) & rop_nr(0x6)
        return 0x00060004;

    case 0x070000:  // nv_wait
        // used in nv50_instmem.c, nv84_instmem_flush
        // return 0;
        break;

    // PTIMER
    // Crystal freq is 27000KHz
    // We use CPU clock value instead of crystal of NVIDIA
    case NV04_PTIMER_TIME_0:  // 0x9400
        // low
        return (uint32_t)timer_now();
    case NV04_PTIMER_TIME_1:  // 0x9410
        // high
        return timer_now() >> 32;
    case NV04_PTIMER_NUMERATOR:  // 0x9200
        return timer_numerator;
    case NV04_PTIMER_DENOMINATOR:  // 0x9210
        return timer_denominator;

    // nvc0_graph.c
    case 0x40910c:  // write 0
        break;
    case 0x409100:  // write 2
        break;
    case 0x409800:  // nv_wait
        // used in nvc0_graph.c nvc0_graph_init_ctxctl
        // HUB init
        //
        // FIXME(Yusuke Suzuki) we should store valid context value...
        // return 0x80000001;
        break;

    // nvc0_vm.c
    case 0x100cb8:
        break;
    case 0x100cbc:
        break;
    case 0x100c80:
        // used in nvc0_vm.c nvc0_vm_flush
        // return 0x00ff8000;
        break;

    // tp
    case GPC_UNIT(0, 0x2608):
        // return 0x3;
        break;
    case GPC_UNIT(1, 0x2608):
        // return 0x4;
        break;
    case GPC_UNIT(2, 0x2608):
        // return 0x3;
        break;
    case GPC_UNIT(3, 0x2608):
        // return 0x4;
        break;
    }
    // return read32(state->bar[0].space, offset);
    return read32(state->bar[0].real, offset);
}

static void quadro6000_mmio_bar0_writed(void *opaque, target_phys_addr_t addr, uint32_t val) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[0].addr;
    switch (offset) {
    case 0x00000000:
        Q6_PRINTF("write call 0x%X\n", val);
        return;

    case 0x070000:  // nv_wait
        // used in nv50_instmem.c, nv84_instmem_flush
        // code
        //     nv_wr32(dev, 0x070000, 0x00000001);
        //     if (!nv_wait(dev, 0x070000, 0x00000002, 0x00000000))
        //           NV_ERROR(dev, "PRAMIN flush timeout\n");
        // flush instruction
        // nv84_instmem_flush();
        // return;
        break;

    case NV04_PTIMER_NUMERATOR:
        timer_numerator = val;
        Q6_PRINTF("numerator set\n");
        return;

    case NV04_PTIMER_DENOMINATOR:
        timer_denominator = val;
        Q6_PRINTF("denominator set\n");
        return;
    }
    // fallback
    // write32(state->bar[0].space, offset, val);
    write32(state->bar[0].real, offset, val);
}

// BAR 1:
//   VRAM. On pre-NV50, corresponds directly to the available VRAM on card.
//   On NV50, gets remapped through VM engine.
static void quadro6000_init_bar1(quadro6000_state_t* state) {
    if (!(state->bar[1].space = qemu_mallocz(0x8000000))) {
        Q6_PRINTF("BAR1 Initialization failed\n");
    }
}

static uint32_t quadro6000_mmio_bar1_readb(void *opaque, target_phys_addr_t addr) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[1].addr;
    // return read8(state->bar[1].space, offset);
    return read8(state->bar[1].real, offset);
}

static uint32_t quadro6000_mmio_bar1_readw(void *opaque, target_phys_addr_t addr) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[1].addr;
    // return read16(state->bar[1].space, offset);
    return read16(state->bar[1].real, offset);
}

static uint32_t quadro6000_mmio_bar1_readd(void *opaque, target_phys_addr_t addr) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[1].addr;
    // return read32(state->bar[1].space, offset);
    return read32(state->bar[1].real, offset);
}

static void quadro6000_mmio_bar1_writeb(void *opaque, target_phys_addr_t addr, uint32_t val) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[1].addr;
    // write8(state->bar[1].space, offset, val);
    write8(state->bar[1].real, offset, val);
}

static void quadro6000_mmio_bar1_writew(void *opaque, target_phys_addr_t addr, uint32_t val) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[1].addr;
    // write16(state->bar[1].space, offset, val);
    write16(state->bar[1].real, offset, val);
}

static void quadro6000_mmio_bar1_writed(void *opaque, target_phys_addr_t addr, uint32_t val) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[1].addr;
    // write32(state->bar[1].space, offset, val);
    write32(state->bar[1].real, offset, val);
}

// BAR3 ramin bar
static void quadro6000_init_bar3(quadro6000_state_t* state) {
    if (!(state->bar[3].space = qemu_mallocz(0x4000000))) {
        Q6_PRINTF("BAR3 Initialization failed\n");
    }
}

static uint32_t quadro6000_mmio_bar3_readb(void *opaque, target_phys_addr_t addr) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[3].addr;
    // return read8(state->bar[3].space, offset);
    return read8(state->bar[3].real, offset);
}

static uint32_t quadro6000_mmio_bar3_readw(void *opaque, target_phys_addr_t addr) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[3].addr;
    // return read16(state->bar[3].space, offset);
    return read16(state->bar[3].real, offset);
}

static uint32_t quadro6000_mmio_bar3_readd(void *opaque, target_phys_addr_t addr) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[3].addr;
    // return read32(state->bar[3].space, offset);
    return read32(state->bar[3].real, offset);
}

static void quadro6000_mmio_bar3_writeb(void *opaque, target_phys_addr_t addr, uint32_t val) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[3].addr;
    // write8(state->bar[3].space, offset, val);
    write8(state->bar[3].real, offset, val);
}

static void quadro6000_mmio_bar3_writew(void *opaque, target_phys_addr_t addr, uint32_t val) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[3].addr;
    // write16(state->bar[3].space, offset, val);
    write16(state->bar[3].real, offset, val);
}

static void quadro6000_mmio_bar3_writed(void *opaque, target_phys_addr_t addr, uint32_t val) {
    quadro6000_state_t* state = (quadro6000_state_t*)(opaque);
    const target_phys_addr_t offset = addr - state->bar[3].addr;
    // write32(state->bar[3].space, offset, val);
    write32(state->bar[3].real, offset, val);
}

// function to access byte (index 0), word (index 1) and dword (index 2)
typedef CPUReadMemoryFunc* CPUReadMemoryFuncBlock[3];
static CPUReadMemoryFuncBlock mmio_read_table[5] = {
    {
        quadro6000_mmio_bar0_readb,
        quadro6000_mmio_bar0_readw,
        quadro6000_mmio_bar0_readd,
    },
    {
        quadro6000_mmio_bar1_readb,
        quadro6000_mmio_bar1_readw,
        quadro6000_mmio_bar1_readd,
    },
    {},  // bar2
    {
        quadro6000_mmio_bar3_readb,
        quadro6000_mmio_bar3_readw,
        quadro6000_mmio_bar3_readd,
    }
};

typedef CPUWriteMemoryFunc* CPUWriteMemoryFuncBlock[3];
static CPUWriteMemoryFuncBlock mmio_write_table[5] = {
    {
        quadro6000_mmio_bar0_writeb,
        quadro6000_mmio_bar0_writew,
        quadro6000_mmio_bar0_writed,
    },
    {
        quadro6000_mmio_bar1_writeb,
        quadro6000_mmio_bar1_writew,
        quadro6000_mmio_bar1_writed,
    },
    {},  // bar2
    {
        quadro6000_mmio_bar3_writeb,
        quadro6000_mmio_bar3_writew,
        quadro6000_mmio_bar3_writed,
    }
};

static void quadro6000_mmio_map(PCIDevice *dev, int region_num, uint32_t addr, uint32_t size, int type) {
    int ret;
    quadro6000_state_t* state = (quadro6000_state_t*)dev;
    const int io_index = cpu_register_io_memory(0, mmio_read_table[region_num], mmio_write_table[region_num], dev);

    quadro6000_bar_t* bar = &(state)->bar[region_num];
    bar->io_index = io_index;
    bar->addr = addr;
    bar->size = size;
    bar->type = type;

    // get MMIO virtual address to real devices
    if (!bar->real) {
        ret = pci_device_map_range(
                state->access,
                state->access->regions[region_num].base_addr,
                state->access->regions[region_num].size,
                PCI_DEV_MAP_FLAG_WRITABLE,
                (void**)&bar->real);
        if (ret) {
            Q6_PRINTF("failed to map virt addr %d\n", ret);
        }
    }

    cpu_register_physical_memory(addr, size, io_index);

    Q6_PRINTF("BAR%d MMIO 0x%X - 0x%X, size %d, io index 0x%X\n", region_num, addr, addr + size, size, io_index);
}

void quadro6000_init_mmio(quadro6000_state_t* state) {
    // Region 0: Memory at d8000000 (32-bit, non-prefetchable) [disabled] [size=32M]
    pci_register_io_region(&state->pt_dev.dev, 0, 0x2000000, PCI_ADDRESS_SPACE_MEM, quadro6000_mmio_map);
    quadro6000_init_bar0(state);

    // Region 1: Memory at c0000000 (64-bit, prefetchable) [disabled] [size=128M]
    pci_register_io_region(&state->pt_dev.dev, 1, 0x8000000, PCI_ADDRESS_SPACE_MEM_PREFETCH, quadro6000_mmio_map);
    quadro6000_init_bar1(state);

    // Region 3: Memory at cc000000 (64-bit, prefetchable) [disabled] [size=64M]
    pci_register_io_region(&state->pt_dev.dev, 3, 0x4000000, PCI_ADDRESS_SPACE_MEM_PREFETCH, quadro6000_mmio_map);
    quadro6000_init_bar3(state);
}
/* vim: set sw=4 ts=4 et tw=80 : */