/*
 * Cross Context
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
#include <cstdlib>
#include <iostream>
#include <boost/asio.hpp>
#include <unistd.h>
#include "a3.h"
#include "a3_session.h"
#include "a3_context.h"
#include "a3_device.h"
#include "a3_bit_mask.h"
#include "a3_registers.h"
#include "a3_barrier.h"
#include "a3_pramin.h"
#include "a3_playlist.h"
#include "a3_device_bar1.h"
#include "a3_shadow_page_table.h"
namespace a3 {


context::context(boost::asio::io_service& io_service, bool through)
    : session(io_service)
    , through_(through)
    , accepted_(false)
    , id_()
    , bar1_channel_(new channel(-1))
    , bar3_channel_(new channel(-3))
    , channels_()
    , barrier_()
    , fifo_playlist_(new playlist())
    , poll_area_(0)
    , reg_(new uint32_t[32ULL * 1024 * 1024])
    , ramin_channel_map_() {
    for (std::size_t i = 0, iz = channels_.size(); i < iz; ++i) {
        channels_[i].reset(new channel(i));
    }
}

context::~context() {
    if (accepted_) {
        device::instance()->release_virt(id_);
        A3_LOG("END and release GPU id %u\n", id_);
    }
}

void context::accept() {
    accepted_ = true;
    id_ = device::instance()->acquire_virt();
    barrier_.reset(new barrier::table(get_address_shift(), vram_size()));
}

// main entry
void context::handle(const command& cmd) {
    if (cmd.type == command::TYPE_INIT) {
        domid_ = cmd.value;
        A3_LOG("INIT domid %d & GPU id %u\n", domid(), id());
        return;
    }

    if (through()) {
        A3_SYNCHRONIZED(device::instance()->mutex_handle()) {
            // through mode. direct access
            const uint32_t bar = cmd.bar();
            if (cmd.type == command::TYPE_WRITE) {
                device::instance()->write(bar, cmd.offset, cmd.value, cmd.size());
            } else if (cmd.type == command::TYPE_READ) {
                buffer()->value = device::instance()->read(bar, cmd.offset, cmd.size());
            }
        }
        return;
    }

    if (cmd.type == command::TYPE_WRITE) {
        switch (cmd.bar()) {
        case command::BAR0:
            write_bar0(cmd);
            break;
        case command::BAR1:
            write_bar1(cmd);
            break;
        case command::BAR3:
            write_bar3(cmd);
            break;
        }
        return;
    }

    if (cmd.type == command::TYPE_READ) {
        switch (cmd.bar()) {
        case command::BAR0:
            read_bar0(cmd);
            break;
        case command::BAR1:
            read_bar1(cmd);
            break;
        case command::BAR3:
            read_bar3(cmd);
            break;
        }
        return;
    }
}

void context::fifo_playlist_update(uint32_t reg_addr, uint32_t cmd) {
    const uint64_t address = get_phys_address(bit_mask<28, uint64_t>(reg_addr) << 12);
    const uint32_t count = bit_mask<8, uint32_t>(cmd);
    const uint64_t shadow = fifo_playlist()->update(this, address, count);
    device::instance()->try_acquire_gpu(this);
    // registers::write32(0x70000, 1);
    registers::write32(0x2270, shadow >> 12);
    registers::write32(0x2274, cmd);
}

void context::flush_tlb(uint32_t vspace, uint32_t trigger) {
    const uint64_t page_directory = get_phys_address(bit_mask<28, uint64_t>(vspace >> 4) << 12);
    const uint64_t vspace_phys = bit_clear<4, uint32_t>(vspace) | static_cast<uint32_t>(page_directory >> 8);

    bool bar1 = false;
    bool bar1_only = true;

    A3_LOG("TLB flush 0x%" PRIX64 " pd\n", page_directory);

    // rescan page tables
    if (bar1_channel()->table()->page_directory_address() == page_directory) {
        // BAR1
        bar1 = true;
        bar1_channel()->table()->refresh_page_directories(this, page_directory);
        A3_SYNCHRONIZED(device::instance()->mutex_handle()) {
            device::instance()->bar1()->shadow(this);
        }
    }

    if (bar3_channel()->table()->page_directory_address() == page_directory) {
        // BAR3
        bar1_only = false;
        bar3_channel()->table()->refresh_page_directories(this, page_directory);
    }
    for (std::size_t i = 0, iz = channels_.size(); i < iz; ++i) {
        if (channels(i)->enabled()) {
            if (channels(i)->table()->page_directory_address() == page_directory) {
                bar1_only = false;
                channels(i)->table()->refresh_page_directories(this, page_directory);
            }
        }
    }

    if (bar1) {
        device::instance()->bar1()->flush();
        if (bar1_only) {
            return;
        }
    }

    registers::accessor registers;
    registers.write32(0x100cb8, vspace_phys);
    registers.write32(0x100cbc, trigger);
}

}  // namespace a3
/* vim: set sw=4 ts=4 et tw=80 : */