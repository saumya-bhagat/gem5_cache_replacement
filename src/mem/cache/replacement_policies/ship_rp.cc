/**
 * SHIP Implementation
 *
 */

#include "mem/cache/replacement_policies/ship_rp.hh"

#include <cassert>
#include <memory>
#include "debug/ship_rp.hh"
#include "base/logging.hh"
#include "base/random.hh"
#include <bitset>
#include "params/SHIPRP.hh"



SHIPRP::SHIPRP(const Params *p): BaseReplacementPolicy(p), 
signature_type(p->signature_type),
numSHCTBits(p->num_SHCT_bits),
numRRPVBits(p->num_bits),
hitPriority(p->hit_priority), btp(p->btp), 
signature_history_counter_array{}
{
    fatal_if(p->num_bits <= 0, "num_bits should be greater than zero.\n");
}

ushort SHIPRP::hash_function(const Addr address_tag) const{
    
    Addr address = address_tag;
    address = address >> 8;
    uint16_t hashed_pc=0;
    std::bitset<4> temp;
    for(int i=0; i<14; i++) {
        temp=address & 0xF;
        if(temp.count() & 1)
            hashed_pc|=(1<<i);
       
        address>>=4;    
    }
    return hashed_pc;
}

void
SHIPRP::invalidate(const std::shared_ptr<ReplacementData>& replacement_data)
const
{
    std::shared_ptr<SHIPReplData> casted_replacement_data =
        std::static_pointer_cast<SHIPReplData>(replacement_data);

    // Set RRPV to an invalid distance
    casted_replacement_data->valid = false;
}

///hit function 
void
SHIPRP::touch(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    std::shared_ptr<SHIPReplData> casted_replacement_data =
        std::static_pointer_cast<SHIPReplData>(replacement_data);
    casted_replacement_data->outcome = true;
    
    ushort index;
    
    if(signature_type) {
        if(replacement_data->pc != 0) {
            index = hash_function(replacement_data->pc); //14 bit hashed PC signature
            //index = (replacement_data->pc) & ((1<<14)-1); //14 bit lower PC signature
            DPRINTF(ship_rp, "HIT::PC signature = %x, SHCT[%x]: %d\n",replacement_data->pc, index, signature_history_counter_array[index]);
            if(signature_history_counter_array[index] < (1<<numSHCTBits)-1)
                signature_history_counter_array[index]++;
        } 
    }
    else
    {
        index = replacement_data->tag & ((1<<14)-1); //14bit memory signature
        DPRINTF(ship_rp, "HIT::Memory signature = %x, SHCT[%x]: %d\n",replacement_data->tag, index, signature_history_counter_array[index]);
        if(signature_history_counter_array[index] < (1<<numSHCTBits)-1)
            signature_history_counter_array[index]++;
    }
    casted_replacement_data->rrpv--;
    DPRINTF(ship_rp, "HIT:: SHCT[%x]: %d, RRPV: %d\n", index, signature_history_counter_array[index], casted_replacement_data->rrpv.operator uint16_t());

    
}

void
SHIPRP::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{   
    bool invalid_pc = false;
    std::shared_ptr<SHIPReplData> casted_replacement_data =
        std::static_pointer_cast<SHIPReplData>(replacement_data);

    casted_replacement_data->outcome = false;
    if(signature_type) //signature is PC type
    {
        if(replacement_data->pc != 0)
            //casted_replacement_data->signature = (replacement_data->pc) & ((1<<14)-1);
           casted_replacement_data->signature = hash_function(replacement_data->pc);
        else
            invalid_pc = true;
    }   
    else              //signature is MEM type
        casted_replacement_data->signature = (unsigned short)(replacement_data->tag & ((1<<14)-1));

    casted_replacement_data->rrpv.saturate();
    if(!invalid_pc)
    {
        ushort signature = casted_replacement_data->signature;
        if (signature_history_counter_array[signature]!=0) { //Block has been accessed before
            casted_replacement_data->rrpv--;
        }
        // Mark entry as ready to be used
        DPRINTF(ship_rp, "MISS::SHCT[%x]: %u\n",signature, signature_history_counter_array[signature]);
        casted_replacement_data->valid = true;
    }
    
}

ReplaceableEntry*
SHIPRP::getVictim(const ReplacementCandidates& candidates) const
{
  // There must be at least one replacement candidate
    assert(candidates.size() > 0);

    // Use first candidate as dummy victim
    ReplaceableEntry* victim = candidates[0];

    // Store victim->rrpv in a variable to improve code readability
    int victim_RRPV = std::static_pointer_cast<SHIPReplData>(
                        victim->replacementData)->rrpv;

    // Visit all candidates to find victim
    for (const auto& candidate : candidates) {
        std::shared_ptr<SHIPReplData> candidate_repl_data =
            std::static_pointer_cast<SHIPReplData>(
                candidate->replacementData);

       // Stop searching for victims if an invalid entry is found
        if (!candidate_repl_data->valid) {
            return candidate;
        }

        // Update victim entry if necessary
        int candidate_RRPV = candidate_repl_data->rrpv;
        if (candidate_RRPV > victim_RRPV) {
            victim = candidate;
            victim_RRPV = candidate_RRPV;
        }
    }

    // Get difference of victim's RRPV to the highest possible RRPV in
    // order to update the RRPV of all the other entries accordingly
    int diff = std::static_pointer_cast<SHIPReplData>(
        victim->replacementData)->rrpv.saturate();

    // No need to update RRPV if there is no difference
    if (diff > 0){
        // Update RRPV of all candidates
        for (const auto& candidate : candidates) {
            std::static_pointer_cast<SHIPReplData>(
                candidate->replacementData)->rrpv += diff;
        }
    }

std::shared_ptr<SHIPReplData> casted_replacement_data =
        std::static_pointer_cast<SHIPReplData>(victim->replacementData);

    if (!casted_replacement_data->outcome) {
        ushort index = casted_replacement_data->signature;
        DPRINTF(ship_rp, "REPLACE:: Signature: %x, SHCT_entry: %d, RRPV: %d\n", index, signature_history_counter_array[index], casted_replacement_data->rrpv.operator uint16_t());
        if (signature_history_counter_array[index] > 0) {
         signature_history_counter_array[index]-- ;   
        }  
        
    }

    return victim;
}

std::shared_ptr<ReplacementData>
SHIPRP::instantiateEntry()
{
    return std::shared_ptr<ReplacementData>(new SHIPReplData(numRRPVBits));
}

SHIPRP*
SHIPRPParams::create()
{
    return new SHIPRP(this);
}

