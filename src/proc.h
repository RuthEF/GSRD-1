
#ifndef PROC_H
#define PROC_H

#include "data.h"


/***/

extern Bool32 procInitAcc (size_t f);

//extern void procBindData (const HostBuff * const pHB, const ParamVal * const pP, const ImgOrg * const pO, const U32 iS);
extern void procFrameStat (BlockStat * const pS, const Scalar * const pAB, const ImgOrg * const pO);

extern U32 proc2I1A (Scalar * restrict pR, Scalar * restrict pS, const ImgOrg * pO, const ParamVal * pP, const U32 iM);

#endif // PROC_H
