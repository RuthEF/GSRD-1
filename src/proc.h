
#ifndef PROC_H
#define PROC_H

#include "data.h"


/***/

extern Bool32 procInitAcc (size_t f);

extern void procBindData (const HostBuff * const pHB, const ParamVal * const pP, const ImgOrg * const pO, const U32 iS);

extern U32 proc (Scalar * restrict pR, const Scalar * restrict pS, const ImgOrg * pO, const ParamVal * pP, const U32 iterMax);
   
#endif // PROC_H
