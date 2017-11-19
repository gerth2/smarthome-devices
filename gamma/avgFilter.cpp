/*
 * A pure-C averaging filter implementation.
 *
 * Change Notes:
 *  Chris Gerth - 2017-11-19
 *     Created
 *  
 */


#include "avgFilter.h"
#include "Arduino.h"

/* init's the object for the filter                         */
/*  in = object for this filter                             */
void AvgFilter_init(FilterData * in){
    memset(in->buf, 0, sizeof(in->buf));
    in->cur_idx = 0;
    in->accum = 0;
}

/* O(1) Filter algorithm. Keeps track of the sum of all     */
/*  numbers in the buffer, adding or removing quantities as */
/*  needed.                                                 */
/*  in = object for this filter                             */
/*  data = input sample for the filter                      */
/*  returns the present output sample for the filter.       */
float AvgFilter_filter(FilterData * in, float data){
    in->accum -= in->buf[in->cur_idx];
    in->buf[in->cur_idx] = data;
    in->accum += data;

    in->cur_idx = (in->cur_idx + 1) % FILTER_SIZE;

    return (in->accum/((float)(FILTER_SIZE)));
}

