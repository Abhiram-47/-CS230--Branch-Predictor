#ifndef LOOP_PREDICTOR_H
#define LOOP_PREDICTOR_H

#include <modules.h>
#include "msl/fwcounter.h"


#define NUM_WAYS        4           // associativity of the loop predictor
#define NUM_ENTRIES     256         // #entries in the loop predictor
#define INIT_AGE        255         // initial age of the entry in the loop predictor
#define TAG_WIDTH       14
#define ITER_WIDTH      14
#define NUM_SETS        64          // =NUM_ENTRIES/NUM_WAYS
#define INDEX_WIDTH     6           // =log2(NUM_SETS)

typedef struct loop_entry {
    champsim::msl::fwcounter<ITER_WIDTH> past_iter_cnt;     // 14-bits
    champsim::msl::fwcounter<ITER_WIDTH> curr_iter_cnt;     // 14-bits
    uint32_t tag;                                           // 14-bits
    champsim::msl::fwcounter<2> confidence;
    champsim::msl::fwcounter<8> age;

    void reset() {
        past_iter_cnt=0;
        curr_iter_cnt=0;
        tag=0;
        confidence=0;
        age=0;
    }
} loop_entry_t;


struct LoopBranchInfo {

    bool hit;
    uint32_t index;
    uint32_t way;
    bool prediction; 
    bool is_valid_loop_prediction;

    void reset() {
        hit = 0;
        index = 0;
        way = 0;
        prediction = 0;
        is_valid_loop_prediction = 0;
    }
};



class LoopPredictor {
    loop_entry_t loop_component[NUM_SETS][NUM_WAYS];


public:

    LoopBranchInfo loop_branch_info;
    bool loop_prediction;


    void loop_predictor_init();
  	bool predict_branch(champsim::address ip);
  	void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);

};



inline void LoopPredictor::loop_predictor_init() {
    for (uint32_t i = 0; i < NUM_SETS; i++)
        for (uint32_t j = 0; j < NUM_WAYS; j++)
            loop_component[i][j].reset();
}


inline bool LoopPredictor::predict_branch(champsim::address ip) {
    uint64_t ip_ = ip.to<unsigned long>();

    uint32_t index = ip_ % NUM_SETS;
    uint32_t tag = (ip_ >> INDEX_WIDTH) & ((1 << TAG_WIDTH) - 1);

    for (uint16_t j = 0; j < NUM_WAYS; j++) {

        loop_entry_t entry = loop_component[index][j];
        if (entry.tag == tag) {
        
            loop_branch_info.hit = true;
            loop_branch_info.index = index;
            loop_branch_info.way = j;
            loop_branch_info.is_valid_loop_prediction = (entry.confidence == 3);

            if (entry.curr_iter_cnt.value() == entry.past_iter_cnt.value()) {
                loop_prediction = false;


                loop_branch_info.prediction = loop_prediction;
                return loop_prediction;
            }

            loop_prediction = true;
            loop_branch_info.prediction = loop_prediction;
            return loop_prediction;
        }
    }

    loop_branch_info.hit = false;
    loop_branch_info.is_valid_loop_prediction = false;
    loop_prediction = false;
    return loop_prediction;

}


inline void LoopPredictor::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type) {


    uint64_t ip_ = ip.to<unsigned long>();


    if (!loop_branch_info.hit) {
        if (!taken) return;         // not-taken miss → never a loop back-edge, don't allocate
        uint32_t index = ip_ % NUM_SETS;
        uint32_t tag = (ip_ >> INDEX_WIDTH) & ((1 << TAG_WIDTH) - 1);

        bool alloc = false;
        for (uint32_t j = 0; j < NUM_WAYS; j++) {
            if (loop_component[index][j].age == 0) {
                loop_component[index][j].past_iter_cnt = 0;
                loop_component[index][j].curr_iter_cnt = 1;             // taken is guaranteed here
                loop_component[index][j].confidence = 0;
                loop_component[index][j].tag = tag;
                loop_component[index][j].age = INIT_AGE;

                alloc = true;
                break;
            }
        }

        if (!alloc)
            for (uint32_t j = 0; j < NUM_WAYS; j++)
                loop_component[index][j].age -= 1;
        
        return;
    }


    uint32_t index = loop_branch_info.index;
    uint32_t way = loop_branch_info.way;
    bool prediction = loop_branch_info.prediction;
    bool valid = loop_branch_info.is_valid_loop_prediction;
    loop_entry_t &entry = loop_component[index][way];

    if (valid) {
        if (prediction == taken && taken) {
            entry.age += 1;
            entry.curr_iter_cnt += 1;
        } else if (prediction == taken && !taken) {
            entry.age += 1;
            entry.past_iter_cnt = entry.curr_iter_cnt;          // they are equal anyway...
            entry.curr_iter_cnt = 0;
            entry.confidence += 1;                              // it stays at 3 only...
        } else if (prediction != taken && taken) {              // turns out, it's not a regular loop...
            entry.past_iter_cnt = 0;
            entry.curr_iter_cnt = 1;
            entry.age = 0;
            entry.confidence = 0;
        } else if (prediction != taken && !taken) {
            entry.past_iter_cnt = 0;
            entry.curr_iter_cnt = 0;
            entry.age = 0;
            entry.confidence = 0;
        }
        return;
    }
    
    if (taken) {
        if (entry.past_iter_cnt.value() == 0) {        // must be newly allocated entry
            entry.curr_iter_cnt += 1;
        } else if (entry.curr_iter_cnt.value() + 1 > entry.past_iter_cnt.value()) {     // not a regular loop
            entry.past_iter_cnt = 0;
            entry.curr_iter_cnt = 1;
            entry.age = 0;
            entry.confidence = 0;
        } else {
            entry.curr_iter_cnt += 1;
        }
    } else {
        if (entry.past_iter_cnt.value() == 0) {                             // counted the iterations for the 1st time
            entry.past_iter_cnt = entry.curr_iter_cnt;
            entry.curr_iter_cnt = 0;
            entry.confidence += 1;
        } else if (entry.past_iter_cnt.value() == entry.curr_iter_cnt.value()) {    // hmmm... could it be a regular loop?
            entry.past_iter_cnt = entry.curr_iter_cnt;
            entry.curr_iter_cnt = 0;
            entry.confidence += 1;
        } else {                                                    // nah... it's not a regular loop
            entry.past_iter_cnt = 0;
            entry.curr_iter_cnt = 0;
            entry.age = 0;
            entry.confidence = 0;
        }
    }
    
    return;

}

#endif