/**
 * \brief  Complement implementation of pow(double, double)
 * \author Martin Stein
 * \date   2012-07-11
 */

#include <math.h>

double pow(double base, double exp)
{ return __builtin_powl(base, exp); }

