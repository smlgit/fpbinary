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

#define FPBINARY_MAJOR_VER 1
#define FPBINARY_MINOR_VER 4
#define FPBINARY_VERSION_STR                                                   \
    xstr(FPBINARY_MAJOR_VER) "." xstr(FPBINARY_MINOR_VER)

#endif /* FPBINARYVERSION_H_ */
