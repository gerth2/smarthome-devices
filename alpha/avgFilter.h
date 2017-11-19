/*
 * A pure-C averaging filter implementation.
 *
 * Change Notes:
 *  Chris Gerth - 2017-11-19
 *     Created
 *  
 */

#define FILTER_SIZE 100

typedef struct {

    float buf[FILTER_SIZE];
    int cur_idx;
    float accum;

} FilterData;

void AvgFilter_init(FilterData * in);
float AvgFilter_filter(FilterData * in, float data);