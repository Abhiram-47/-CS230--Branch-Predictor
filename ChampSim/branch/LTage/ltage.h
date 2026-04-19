#ifndef BRANCH_LTAGE_H
#define BRANCH_LTAGE_H

#include "tage.h"
#include "loop_predictor.h"


class LTage : champsim::modules::branch_predictor
{

    Tage tage_predictor;
    LoopPredictor loop_predictor;

public:
    using branch_predictor::branch_predictor;
    
    void initialize_branch_predictor();
  	bool predict_branch(champsim::address ip);
  	void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};



#endif