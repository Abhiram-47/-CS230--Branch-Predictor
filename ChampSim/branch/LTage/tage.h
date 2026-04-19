#ifndef BRANCH_TAGE_H
#define BRANCH_TAGE_H

#include <array>
#include <bitset>

#include "modules.h"
#include "msl/fwcounter.h"


#define NUM_COMPONENTS 				13										// including bimodal component
#define TAGE_ENTRIES 				1024									// T1, T2, ... have 512 entries each

#define BIMODAL_ENTRIES				16 * 1024
#define BIMODAL_BITS				2

#define PHIST_BITS					16										// #bits of phist register
#define GHIST_BITS					640										// #bits of ghist register

#define WEAKLY_TAKEN				4
#define WEAKLY_NOT_TAKEN			3

#define RESET_USEFUL_INTERVAL		512 * 1024

#define TAGE_ENTRY_INVALID make_invalid_tage_entry()

// const int32_t TAG_WIDTHS[] = {9, 9, 10, 10, 11, 11, 12};
const int32_t TAG_WIDTHS[] = {7, 7, 8, 8, 9, 10, 11, 12, 12, 13, 14, 15};
// const int32_t HIST_LENS[]	= {5, 9, 15, 25, 44, 76, 130};
const int32_t HIST_LENS[]	= {4, 6, 10, 16, 25, 40, 64, 101, 160, 254, 403, 640};
// const int32_t TAGE_INDEX_BITS[] = {9, 9, 9, 9, 9, 9, 9};
const int32_t TAGE_INDEX_BITS[] = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10};



// --------- Entries of TAGE table ---------
typedef struct tage_entry {
	champsim::msl::fwcounter<3> pred;
	int32_t tag;															// max width of tag is TAG_WIDTHS[-1] < 32 bits
	champsim::msl::fwcounter<2> u;

	void reset() {
		pred = 0;
		tag = 0;
		u = 0;
	};
} tage_entry_t;


inline tage_entry_t make_invalid_tage_entry()
{
    tage_entry_t e;
    e.pred = champsim::msl::fwcounter<3>{0};
    e.tag  = -1;
    e.u    = champsim::msl::fwcounter<2>{0};
    return e;
}
// -----------------------------------------


// struct BranchInfo_v0 {
	
// 	champsim::address branch_ip;

// 	int32_t hit_bank;
// 	int32_t hit_bank_index;
// 	int32_t alt_bank;
// 	int32_t alt_bank_index;
// 	int32_t bimodal_index;

// 	bool tage_pred;
// 	bool alt_taken;
// 	bool longest_match_pred;

// 	void reset() {
// 		branch_ip = -1;
// 		hit_bank = -1;
// 		hit_bank_index = -1;
// 		alt_bank = -1;
// 		alt_bank_index = -1;
// 		bimodal_index = -1;
// 		tage_pred = -1;
// 		alt_taken = -1;
// 		longest_match_pred = -1;
// 	}

// };

struct BranchInfo {
	champsim::address branch_ip;

	int32_t provider_comp;
	tage_entry_t provider_comp_entry;
	bool provider_comp_pred;
	int32_t provider_index;

	int32_t altpred_comp;
	tage_entry_t altpred_comp_entry;
	bool altpred_comp_pred;
	int32_t altpred_index;

	bool ultimate_pred;

	champsim::msl::fwcounter<BIMODAL_BITS> bimodal_entry;

	void reset() {
		branch_ip = champsim::address{static_cast<uint64_t>(-1)};;
		provider_comp = -1;
		altpred_comp = -1;

		provider_index = -1;
		altpred_index = -1;


		provider_comp_pred = -1;
		altpred_comp_pred = -1;
		ultimate_pred = -1;

		provider_comp_entry.reset();
		altpred_comp_entry.reset();
		bimodal_entry = 2;
	}
};


class Tage
{

	tage_entry_t tage_table[NUM_COMPONENTS - 1][TAGE_ENTRIES];
	std::array<champsim::msl::fwcounter<BIMODAL_BITS>, BIMODAL_ENTRIES> bimodal_table;

	struct BranchInfo branch_info;

	int32_t num_branches;

	int32_t phist;
	std::bitset<GHIST_BITS + 1> ghist;

	champsim::msl::fwcounter<4> USE_ALT_ON_NA;

public:
	// using branch_predictor::branch_predictor;


	uint32_t indexHash(champsim::address ip, int32_t comp);
	uint32_t tagHash(champsim::address ip, int32_t comp);

	uint32_t get_ghist_hash(int32_t inSize, int32_t outSize);

  	void initialize_branch_predictor();
  	bool predict_branch(champsim::address ip);
  	void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};







inline bool Tage::predict_branch(champsim::address ip)
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


inline void Tage::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
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



inline void Tage::initialize_branch_predictor() {

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



inline uint32_t Tage::get_ghist_hash(int32_t inSize, int32_t outSize) {
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


inline uint32_t Tage::tagHash(champsim::address ip, int32_t component) {
    uint32_t ip_ = ip.to<unsigned long>();
    uint32_t global_history_hash = get_ghist_hash(HIST_LENS[component - 1], TAG_WIDTHS[component - 1]);
    global_history_hash ^= (get_ghist_hash(HIST_LENS[component - 1], TAG_WIDTHS[component - 1] - 1) << 1);
    
    // return (ip_ ^ global_history_hash) & ((1 << TAG_WIDTHS[component - 1]) - 1);
    return (ip_ & ((1 << TAG_WIDTHS[component - 1]) - 1)) ^ global_history_hash;
}


inline uint32_t Tage::indexHash(champsim::address ip, int32_t component) {
    uint32_t ip_ = ip.to<unsigned long>();
    // Hash of global history
    uint32_t global_history_hash = get_ghist_hash(HIST_LENS[component - 1], TAGE_INDEX_BITS[component - 1]);

    return (ip_ ^ (ip_ >> TAGE_INDEX_BITS[component - 1]) ^ global_history_hash ^ phist) & ((1 << TAGE_INDEX_BITS[component - 1]) - 1);
}


#endif