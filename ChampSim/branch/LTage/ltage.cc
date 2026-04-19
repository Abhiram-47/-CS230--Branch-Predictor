#include "ltage.h"


bool LTage::predict_branch(champsim::address ip)
{
    bool loop_prediction = loop_predictor.predict_branch(ip);
    bool tage_prediction = tage_predictor.predict_branch(ip);

    if (loop_predictor.loop_branch_info.is_valid_loop_prediction)
        return loop_prediction;
    
    return tage_prediction;
}


void LTage::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
    loop_predictor.last_branch_result(ip, branch_target, taken, branch_type);
    tage_predictor.last_branch_result(ip, branch_target, taken, branch_type);
}


void LTage::initialize_branch_predictor()
{
    loop_predictor.loop_predictor_init();
    tage_predictor.initialize_branch_predictor();
}