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

#include "mem/cache/replacement_policies/drrip_rp.hh"
#include <cstdlib>
#include <iostream>

#include "base/intmath.hh"
#include "base/logging.hh"
#include <memory>
#include <bitset>
#include "base/random.hh"
#include "params/DRRIPRP.hh"
#include "debug/PSEL_value.hh"

DRRIPRP::DRRIPRP(const Params *p)
    :BRRIPRP(p), 
     PSEL(p->PSEL_width, 1<<(p->PSEL_width-1)),
     K(p->K),
     N(p->size / (p->block_size * p->assoc)),
     num_offset_bits(floorLog2(N/K)),
     num_constituency_bits(floorLog2(K))
{
}


void
DRRIPRP::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    uint64_t setidx = replacement_data->setindex;
    SET_TYPE _set_type;
    
    
    uint32_t smaller = (num_offset_bits<num_constituency_bits) ? num_offset_bits : num_constituency_bits;
    uint32_t offset = setidx & ((1<<smaller)-1);
    uint32_t constituency = (setidx>>smaller) & ((1<<smaller)-1);

    if(constituency == offset)
        _set_type = DEDICATE_RRIP;
    else if(constituency == (offset ^ ((1<<smaller)-1))) 
        _set_type = DEDICATE_BRRIP;
    else
        _set_type = FOLLOWER;  

    std::shared_ptr<BRRIPReplData> casted_replacement_data =
        std::static_pointer_cast<BRRIPReplData>(replacement_data);    

    switch(_set_type) {
        case FOLLOWER: 
            DPRINTF(PSEL_value, "FOLLOWER: PSEL = %d\n", PSEL);
            if(PSEL.calcSaturation() > 0.5) 
                BRRIPRP::reset(replacement_data);
            else 
               {
                casted_replacement_data->rrpv.saturate();

                casted_replacement_data->rrpv--;

                // Mark entry as ready to be used
                casted_replacement_data->valid = true;
               }
            
            break;

        case DEDICATE_RRIP:
            PSEL++;
            
            DPRINTF(PSEL_value, "RRIP: PSEL = %d\n", PSEL);
            casted_replacement_data->rrpv.saturate();

            casted_replacement_data->rrpv--;
            // Mark entry as ready to be used
            casted_replacement_data->valid = true;
            
            break;
        case DEDICATE_BRRIP:
            PSEL--;
            DPRINTF(PSEL_value, "BRRIP: PSEL = %d\n", PSEL);
            BRRIPRP::reset(replacement_data);
            break;

    }
}

DRRIPRP*
DRRIPRPParams::create()
{
    return new DRRIPRP(this);
}
