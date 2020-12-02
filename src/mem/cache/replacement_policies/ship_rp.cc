/**
 * SHIP Implementation based on research paper.
 *
 */

#include "mem/cache/replacement_policies/ship_rp.hh"

#include <cassert>
#include <memory>
#include "debug/Cacheset.hh"
#include "base/logging.hh" // For fatal_if
#include "base/random.hh"
#include <bitset>
#include "params/SHIPRP.hh"
/*#include <iostream>
#include <fstream>*/


SHIPRP::SHIPRP(const Params *p): BaseReplacementPolicy(p), numRRPVBits(p->num_bits),
hitPriority(p->hit_priority), btp(p->btp), 
signature_history_counter_array{}
{
    fatal_if(p->num_bits <= 0, "num_bits should be greater than zero.\n");
}

ushort SHIPRP::hash_function(const Addr address_tag) const{
    //std::cout << "Given address is " << address_tag << std::endl;
    /*Addr address_64_bit = address_tag;
    
    std::bitset<64> k(address_64_bit);

    uint64_t shifted1 = address_64_bit >> 14;
    std::bitset<14> first_14(shifted1);
    
    uint64_t shifted2 = shifted1 >> 14;
    std::bitset<14> second_14(shifted2);
   
    uint64_t shifted3 = shifted2 >> 14;
    std::bitset<14> third_14(shifted3);
   
    std::bitset<14> final_xor_value;
    final_xor_value = first_14 ^ second_14 ^ third_14;
    unsigned short index = (unsigned short)final_xor_value.to_ulong() ;
    
   return index;
    */
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
    //std::cout << "Touched" << std::endl;
    std::shared_ptr<SHIPReplData> casted_replacement_data =
        std::static_pointer_cast<SHIPReplData>(replacement_data);
    casted_replacement_data->outcome = true;
    
    ushort index = hash_function(replacement_data->pc);
    //ushort index = replacement_data->tag;
    int prev_value = signature_history_counter_array[index];
   
    if (prev_value < 7) {
        prev_value++;
    }
    signature_history_counter_array[index] = prev_value;
    

    casted_replacement_data->rrpv--;
    DPRINTF(Cacheset, "HIT::pc = %x, SHCT[%x]: %d\n",replacement_data->pc, index, signature_history_counter_array[index]);
}

void
SHIPRP::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    std::shared_ptr<SHIPReplData> casted_replacement_data =
        std::static_pointer_cast<SHIPReplData>(replacement_data);

    casted_replacement_data->outcome = false;
    casted_replacement_data->signature = hash_function(replacement_data->pc);
    //casted_replacement_data->signature = (unsigned short)replacement_data->tag;

    ushort signature = casted_replacement_data->signature;
    casted_replacement_data->rrpv.saturate();
    
    if (signature_history_counter_array[signature]!=0) { //Block has been accessed before
        casted_replacement_data->rrpv--;
    }
    // Mark entry as ready to be used
    casted_replacement_data->valid = true;
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
        int value = signature_history_counter_array[index];
        if (value > 0) {
        //std::cout << "--------------Decrementing--------------" << std::endl;
         value--;   
        }   
        signature_history_counter_array[index] = value;
        DPRINTF(Cacheset, "REPLACE:: Tag: %x, SHCT_entry: %d\n", index, signature_history_counter_array[index]);
        //std ::cout << "ohh index " << "   "<< index << "  " 
        //<< signature_history_counter_array[index] << std::endl;
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

