/*
 * Functions to generate random variation using the on-chip RNG header
 */

#ifndef RANDOM_ADJUST_H
#define RANDOM_ADJUST_H

/* Function declarations */
void random_init(void);
int32_t random_percentage_adjust(uint8_t size, uint8_t max, int32_t current,
        int32_t initial);


#endif /* RANDOM_ADJUST_H */
