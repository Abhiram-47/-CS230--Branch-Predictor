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


class Tage : champsim::modules::branch_predictor
{

	tage_entry_t tage_table[NUM_COMPONENTS - 1][TAGE_ENTRIES];
	std::array<champsim::msl::fwcounter<BIMODAL_BITS>, BIMODAL_ENTRIES> bimodal_table;

	struct BranchInfo branch_info;

	int32_t num_branches;

	int32_t phist;
	std::bitset<GHIST_BITS + 1> ghist;

	champsim::msl::fwcounter<4> USE_ALT_ON_NA;

public:
	using branch_predictor::branch_predictor;


	uint32_t indexHash(champsim::address ip, int32_t comp);
	uint32_t tagHash(champsim::address ip, int32_t comp);

	uint32_t get_ghist_hash(int32_t inSize, int32_t outSize);

  	void initialize_branch_predictor();
  	bool predict_branch(champsim::address ip);
  	void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};


#endif