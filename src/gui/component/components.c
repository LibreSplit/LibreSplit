/** \file components.c
 *
 * Available Components and related utilities
 */
#include "components.h"

LSComponent* ls_component_title_new(void);
LSComponent* ls_component_splits_new(void);
LSComponent* ls_component_timer_new(void);
LSComponent* ls_component_detailed_timer_new(void);
LSComponent* ls_component_prev_segment_new(void);
LSComponent* ls_component_best_sum_new(void);
LSComponent* ls_component_pb_new(void);
LSComponent* ls_component_wr_new(void);

LSComponentAvailable ls_components[] = {
    { "title", ls_component_title_new },
    { "splits", ls_component_splits_new },
    //    { "timer", ls_component_timer_new },
    { "detailed-timer", ls_component_detailed_timer_new },
    { "prev-segment", ls_component_prev_segment_new },
    { "best-sum", ls_component_best_sum_new },
    { "pb", ls_component_pb_new },
    { "wr", ls_component_wr_new },
    { NULL, NULL }
};
