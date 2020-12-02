/**
 * SHIP Implementation based on research paper.
 *
 */
#include "mem/cache/replacement_policies/ship_rp.hh"

#include <cassert>
#include <memory>

#include "base/logging.hh" // For fatal_if
#include "base/random.hh"
#include <bitset>
#include "params/SHIPRP.hh"
/*#include <iostream>
#include <fstream>*/


SHIPRP::SHIPRP(const Params *p): BaseReplacementPolicy(p), num_bits(p->num_bits),
hitPriority(p->hit_priority), btp(p->btp), 
signature_history_counter_array{}
{
    fatal_if(num_bits <= 0, "num_bits should be greater than zero.\n");
}

ushort SHIPRP::hash_function(const Addr address_tag) const{
    //std::cout << "Given address is " << address_tag << std::endl;
    Addr address_64_bit = address_tag;

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

}

void
SHIPRP::invalidate(const std::shared_ptr<ReplacementData>& replacement_data)
const
{
    std::shared_ptr<SHIPReplData> casted_replacement_data =
        std::static_pointer_cast<SHIPReplData>(replacement_data);

    // Set RRPV to an invalid distance
    casted_replacement_data->rrpv = num_bits + 1;
}

///hit function 
void
SHIPRP::touch(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    //std::cout << "Touched" << std::endl;
    std::shared_ptr<SHIPReplData> casted_replacement_data =
        std::static_pointer_cast<SHIPReplData>(replacement_data);
    casted_replacement_data->outcome = true;
    ushort index = hash_function(replacement_data->_tag);
    int prev_value = signature_history_counter_array[index];
   
    if (prev_value < 7) {
        //std::cout << "---------Yes Incrementing--------------" << std::endl; 
        prev_value++;
    }
    signature_history_counter_array[index] = prev_value;
    //std::cout << "Returned address is "<< index << "      " << 
    //signature_history_counter_array[index] << std::endl;
}

void
SHIPRP::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    std::shared_ptr<SHIPReplData> casted_replacement_data =
        std::static_pointer_cast<SHIPReplData>(replacement_data);

    casted_replacement_data->outcome = false;
    casted_replacement_data->signature = hash_function(replacement_data->_tag);

    ushort signature = casted_replacement_data->signature;
    if (signature_history_counter_array[signature]==0) {
        casted_replacement_data->rrpv = num_bits;
    } else {
        casted_replacement_data->rrpv = num_bits-1;
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
        // Get candidate's rrpv
        int candidate_RRPV = std::static_pointer_cast<SHIPReplData>(
                                    candidate->replacementData)->rrpv;

        // Stop searching for victims if an invalid entry is found
        if (candidate_RRPV == num_bits + 1) {
            return candidate;
        // Update victim entry if necessary
        } else if (candidate_RRPV > victim_RRPV) {
            victim = candidate;
            victim_RRPV = candidate_RRPV;
        }
    }

    // Get difference of victim's RRPV to the highest possible RRPV in
    // order to update the RRPV of all the other entries accordingly
    int diff = num_bits - victim_RRPV;

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
        //std ::cout << "ohh index " << "   "<< index << "  " 
        //<< signature_history_counter_array[index] << std::endl;
    }

    return victim;
}

std::shared_ptr<ReplacementData>
SHIPRP::instantiateEntry()
{
    return std::shared_ptr<ReplacementData>(new SHIPReplData(num_bits, false, 
        0));
}

SHIPRP*
SHIPRPParams::create()
{
    return new SHIPRP(this);
}

