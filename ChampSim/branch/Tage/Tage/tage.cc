#include "tage.h"

bool Tage::predict_branch(champsim::address ip)
{
    branch_info.reset();
    
    uint32_t ip_ = ip.to<unsigned long>();

    auto base_pred = bimodal_table[ip.to<unsigned long>() % (BIMODAL_ENTRIES)];
    branch_info.bimodal_entry = base_pred;
    

    std::array<int32_t, NUM_COMPONENTS> tage_index;
    for (int i = 1; i < NUM_COMPONENTS; i++)
        tage_index[i] = indexHash(ip, i);            // TODO: Implement `indexHash`. depends on: ip, ghist, phist, and history length of the ith component
    
    std::array<int32_t, NUM_COMPONENTS> tage_tags;
    for (int i = 1; i < NUM_COMPONENTS; i++)
        tage_tags[i] = tagHash(ip, i);               // TODO: Implement `tagHash`. depends on: ip, ghist, phist, and history length of the ith component
    
    
    int32_t provider_comp = -1, altpred_comp = -1;
    for (int i = 1; i < NUM_COMPONENTS; i++) {
        if (tage_table[i - 1][tage_index[i]].tag == tage_tags[i]) {
            if (provider_comp != -1)
                altpred_comp = provider_comp;
            provider_comp = i;
        }
    }
    branch_info.provider_comp = provider_comp;
    branch_info.altpred_comp = altpred_comp;

    if (provider_comp == -1) {
        branch_info.ultimate_pred = base_pred.value() > (base_pred.maximum / 2);
        return branch_info.ultimate_pred;
    }

    tage_entry_t entry = tage_table[provider_comp - 1][tage_index[provider_comp]];
    bool provider_ctr_prediction = (entry.pred.value() > entry.pred.maximum / 2);
    branch_info.provider_index = tage_index[provider_comp];
    branch_info.provider_comp_pred = provider_ctr_prediction;
    branch_info.provider_comp_entry = entry;

    if (altpred_comp != -1) {
        tage_entry_t alt_entry = tage_table[altpred_comp - 1][tage_index[altpred_comp]];
        bool alt_ctr_prediction = alt_entry.pred.value() > (alt_entry.pred.maximum / 2);
        branch_info.altpred_index = tage_index[altpred_comp];
        branch_info.altpred_comp_pred = alt_ctr_prediction;
        branch_info.altpred_comp_entry = tage_table[altpred_comp - 1][tage_index[altpred_comp]];
    }

    // TODO: Extended it to support the case of dynamic switching b/w altpred 
    // and provider components prediction based on the register `USE_ALT_ON_NA`
    if ( ( (entry.pred.value() == WEAKLY_NOT_TAKEN || entry.pred.value() == WEAKLY_TAKEN) && entry.u == 0 ) && (USE_ALT_ON_NA > 7) ) {       // NEWLY ALLOCATED ENTRY
        if (altpred_comp == -1) {

            branch_info.altpred_comp = -1;

            branch_info.ultimate_pred = base_pred.value() > (base_pred.maximum / 2);
            return branch_info.ultimate_pred;
        }

        bool alt_ctr_prediction = branch_info.altpred_comp_pred;

        branch_info.ultimate_pred = alt_ctr_prediction;
        return branch_info.ultimate_pred;
    }

    branch_info.ultimate_pred = branch_info.provider_comp_pred;
    return branch_info.ultimate_pred;
}


void Tage::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{

    uint32_t ip_ = ip.to<unsigned long>();

    auto bimodal_entry = branch_info.bimodal_entry;
    bool bimodal_prediction = bimodal_entry.value() > (bimodal_entry.maximum / 2);

    bool ultimate_prediction = branch_info.ultimate_pred;

    int32_t provider_comp = branch_info.provider_comp;
    int32_t altpred_comp = branch_info.altpred_comp;

    bool provider_prediction = (provider_comp == -1) ? bimodal_prediction : branch_info.provider_comp_pred;
    bool altpred_prediction = (altpred_comp == -1) ? bimodal_prediction : branch_info.altpred_comp_pred;

    tage_entry_t provider_entry = (provider_comp == -1) ? TAGE_ENTRY_INVALID : branch_info.provider_comp_entry;
    tage_entry_t altpred_entry = (altpred_comp == -1) ? TAGE_ENTRY_INVALID : branch_info.altpred_comp_entry;
    
    int32_t provider_index = (provider_comp == -1) ? -1 : branch_info.provider_index;
    int32_t altpred_index = (altpred_comp == -1) ? -1 : branch_info.altpred_index;


    // update `USE_ALT_ON_NA`
    if (provider_comp != -1) {
        if (tage_table[provider_comp - 1][provider_index].u == 0 && 
            ((tage_table[provider_comp - 1][provider_index].pred == WEAKLY_NOT_TAKEN) ||
                (tage_table[provider_comp - 1][provider_index].pred == WEAKLY_TAKEN))) {        // behaves like a newly allocated entry

            if (provider_prediction != altpred_prediction) {
                if (altpred_prediction == taken)
                    USE_ALT_ON_NA += 1;
                else
                    USE_ALT_ON_NA -= 1;
            }
        }
    }

    // Update the `ctr`
    if (provider_comp != -1) {
        tage_table[provider_comp - 1][provider_index].pred += taken ? 1 : -1;
    } else {
        int32_t bimodal_index = ip.to<unsigned long>() % (BIMODAL_ENTRIES);
        bimodal_table[bimodal_index] += taken ? 1 : -1;
    }

    // L-TAGE improvement
    if (provider_comp != -1) {
        if (tage_table[provider_comp - 1][provider_index].u.value() == 0 && altpred_comp != -1) {
            if (taken)
                tage_table[altpred_comp - 1][altpred_index].pred += 1;
            else
                tage_table[altpred_comp - 1][altpred_index].pred -= 1;
        }
    }

    // Update the `u`
    if (provider_comp != -1) {
        if (ultimate_prediction != altpred_prediction) {
            if (ultimate_prediction == taken)
                tage_table[provider_comp - 1][provider_index].u += 1;
            else
                tage_table[provider_comp - 1][provider_index].u -= 1;
        }
    }

    if (ultimate_prediction != taken && provider_comp < NUM_COMPONENTS - 1) {
        // Allocate a new entry on longer history components
        // TODO: ALLOCATE NEW ENTRY AND AVOID PING-PONG PHENOMENON (IGNORE PING-PONG IN 1ST ITERATION)
        int32_t next_comp = (provider_comp == -1) ? 1 : provider_comp + 1;
        uint32_t rand_ = rand() & ((1 << (NUM_COMPONENTS - provider_comp - 1)) - 1);

        // compute the next component for search 
        // [this random allocation policy is taken from L-TAGE paper and not from TAGE paper]
        if (rand_ & 1) {    // 0.5 probability
            next_comp += 1;
            if (rand_ & 2)  // 0.25 probability
                next_comp += 1;
        }

        bool alloc = false;
        for (int32_t i = next_comp; i < NUM_COMPONENTS; i++) {
            int32_t comp_index = indexHash(ip, i);
            if (tage_table[i - 1][comp_index].u == 0) {
                alloc = true;
                tage_table[i - 1][comp_index].pred = taken ? WEAKLY_TAKEN : WEAKLY_NOT_TAKEN;
                tage_table[i - 1][comp_index].tag = tagHash(ip, i);
                tage_table[i - 1][comp_index].u = 0;
                break;
            }
        }

        if (!alloc)       // Decrement all the `u` counters
            for (int32_t i = next_comp; i < NUM_COMPONENTS; i++)
                tage_table[i - 1][indexHash(ip, i)].u -= 1;
    }


    num_branches++;
    if (num_branches == RESET_USEFUL_INTERVAL / 2) {
        for (int32_t i = 1; i < NUM_COMPONENTS; i++) {
            for (int32_t j = 0; j < TAGE_ENTRIES; j++)
                tage_table[i - 1][j].u = tage_table[i - 1][j].u.value() & 1;
        }
    } else if (num_branches == RESET_USEFUL_INTERVAL) {
        num_branches = 0;
        for (int32_t i = 1; i < NUM_COMPONENTS; i++) {
            for (int32_t j = 0; j < TAGE_ENTRIES; j++)
                tage_table[i - 1][j].u = tage_table[i - 1][j].u.value() & 2;
        }
    }

    // TODO: Update global history and path history
    phist = (phist << 1) | (ip_ & 1);
    phist = (phist & ((1 << PHIST_BITS) - 1));

    ghist = (ghist << 1);
    ghist.set(0, taken);
    
}



void Tage::initialize_branch_predictor() {

    for (int i = 1; i < NUM_COMPONENTS; i++)
        for (int j = 0; j < TAGE_ENTRIES; j++)
            tage_table[i - 1][j].reset();
    
    for (int i = 0; i < BIMODAL_ENTRIES; i++)
        bimodal_table[i] = 1 + bimodal_table[i].maximum / 2;        // initialize bimodal entries to weakly taken
    
    num_branches = 0;
    ghist.reset();
    phist = 0;

    branch_info.reset();
}



uint32_t Tage::get_ghist_hash(int32_t inSize, int32_t outSize) {
    // Compress global history of last 'inSize' branches into 'outSize' by wrapping the history
    uint32_t hash = 0;
    for (int32_t i = 0; i < inSize; i++) {
        if (ghist[i]) {
            // Map bit i into one of outSize positions
            uint32_t pos = i % outSize;
            hash ^= (1ULL << pos);
        }
    }

    return hash;
}


uint32_t Tage::tagHash(champsim::address ip, int32_t component) {
    uint32_t ip_ = ip.to<unsigned long>();
    uint32_t global_history_hash = get_ghist_hash(HIST_LENS[component - 1], TAG_WIDTHS[component - 1]);
    global_history_hash ^= (get_ghist_hash(HIST_LENS[component - 1], TAG_WIDTHS[component - 1] - 1) << 1);
    
    // return (ip_ ^ global_history_hash) & ((1 << TAG_WIDTHS[component - 1]) - 1);
    return (ip_ & ((1 << TAG_WIDTHS[component - 1]) - 1)) ^ global_history_hash;
}


uint32_t Tage::indexHash(champsim::address ip, int32_t component) {
    uint32_t ip_ = ip.to<unsigned long>();
    // Hash of global history
    uint32_t global_history_hash = get_ghist_hash(HIST_LENS[component - 1], TAGE_INDEX_BITS[component - 1]);

    return (ip_ ^ (ip_ >> TAGE_INDEX_BITS[component - 1]) ^ global_history_hash ^ phist) & ((1 << TAGE_INDEX_BITS[component - 1]) - 1);
}