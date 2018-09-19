// Miscellaneous utility functions

#pragma once

#include <hexrays.hpp>

// Produce strings for various objects in the Hex-Rays ecosystem (for 
// debugging / informational purposes)
const char *mopt_t_to_string(mopt_t t);
void mcode_t_to_string(minsn_t *o, char *outBuf, size_t n);
const char *MicroMaturityToString(mba_maturity_t mmt);

// Compare two mop_t objects. Hopefully this will be an official export in the
// microcode API in the future, so we won't have to implement it ourselves.
bool equal_mops_ignore_size(const mop_t &lo, const mop_t &ro);

