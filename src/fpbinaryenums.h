/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

#ifndef FPBINARYENUMS_H_
#define FPBINARYENUMS_H_

#include "fpbinarycommon.h"

extern PyTypeObject OverflowEnumType;
extern PyTypeObject RoundingEnumType;

void fpbinaryenums_InitModule(void);

#endif /* FPBINARYENUMS_H_ */
