
#include "PigeonCommon.h"
#include "PigeonContext.h"

namespace rdmanager{

class vQPMapper{
public:
    vQPMapper() {
    }

    ibv_qp* get_map_qp() {
        // TODO: automatically remove the pigeon that is in error status and add a new one 
        while (pigeon_list_[primary_index_].get_status() == PigeonStatus::PIGEON_STATUS_INIT);
        while (pigeon_list_[primary_index_].get_status() == PigeonStatus::PIGEON_STATUS_ERROR) {
            pigeon_swap(primary_index_, secondary_index_);
            pigeon_debug("primary qp error, switch to secondary qp\n");
        } 
        return pigeon_list_[primary_index_].get_qp();
    }

    ibv_cq* get_map_cq() {
        // TODO: automatically remove the pigeon that is in error status and add a new one 
        while (pigeon_list_[primary_index_].get_status() == PigeonStatus::PIGEON_STATUS_INIT);
        while (pigeon_list_[primary_index_].get_status() == PigeonStatus::PIGEON_STATUS_ERROR) {
            pigeon_swap(primary_index_, secondary_index_);
            pigeon_debug("primary qp error, switch to secondary qp\n");
        }
        return pigeon_list_[primary_index_].get_cq();
    }

    uint32_t get_map_lkey() {
        return pigeon_list_[primary_index_].get_mr()->lkey;
    }

    void add_pigeon_context(PigeonContext* context) {
        pigeon_list_.push_back(*context);
    }

private:
    std::vector<PigeonContext> pigeon_list_ ;
    int primary_index_ = 0;
    int secondary_index_ = 1;

};

}