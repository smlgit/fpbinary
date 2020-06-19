/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * This is where the fpbinary package version is defined.
 * Don't change the format of the defines - they are assumed by setup.py .
 *
 *****************************************************************************/

#ifndef FPBINARYVERSION_H_
#define FPBINARYVERSION_H_

#include "fpbinarycommon.h"

/* MAJOR_VERSION, MINOR_VERSION and MICRO_VERSION need to be set when the comiler
 * is invoked (normally done by setup.py).
 */
#define FPBINARY_VERSION_STR                                                   \
    xstr(MAJOR_VERSION) "." xstr(MINOR_VERSION) "." xstr(MICRO_VERSION)

#endif /* FPBINARYVERSION_H_ */
