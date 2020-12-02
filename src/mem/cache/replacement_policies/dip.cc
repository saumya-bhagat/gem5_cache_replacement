/**
 * Copyright (c) 2018 Inria
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mem/cache/replacement_policies/dip.hh"
#include <cstdlib>
#include <iostream>

#include "base/intmath.hh"
#include "base/logging.hh"
#include <memory>
#include <bitset>
#include "base/random.hh"
#include "params/DIPRP.hh"

DIPRP::DIPRP(const Params *p)
    :BIPRP(p), 
     PSEL(p->PSEL_width, 1<<(p->PSEL_width-1)),
     K(p->K),
     N(p->size / (p->block_size * p->assoc)),
     num_offset_bits(floorLog2(N/K)),
     num_constituency_bits(floorLog2(K))
{
}


void
DIPRP::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    uint64_t setidx = replacement_data->setindex;
    SET_TYPE _set_type;
    
    
    uint32_t smaller = (num_offset_bits<num_constituency_bits) ? num_offset_bits : num_constituency_bits;
    uint32_t offset = setidx & ((1<<smaller)-1);
    uint32_t constituency = (setidx>>smaller) & ((1<<smaller)-1);

    if(constituency == offset)
        _set_type = DEDICATE_LRU;
    else if(constituency == (offset ^ ((1<<smaller)-1))) 
        _set_type = DEDICATE_BIP;
    else
        _set_type = FOLLOWER;  

    switch(_set_type) {
        case FOLLOWER: 
            if(PSEL.calcSaturation() > 0.5) 
                BIPRP::reset(replacement_data);
            else 
                LRURP::reset(replacement_data);
            break;
        case DEDICATE_LRU:
            PSEL++;
            LRURP::reset(replacement_data);
            break;
        case DEDICATE_BIP:
            PSEL--;
            BIPRP::reset(replacement_data);
            break;

    }
}

DIPRP*
DIPRPParams::create()
{
    return new DIPRP(this);
}
