// proc.h - Gray-Scott Reaction-Diffusion using OpenACC
// https://github.com/DrAl-HFS/GSRD.git
// (c) GSRD Project Contributors Feb-April 2018

#ifndef PROC_H
#define PROC_H

#include "data.h"

#define PROC_NOWRAP FALSE

/***/

extern Bool32 procInitAcc (size_t f);
extern const char *procGetCurrAccTxt (char t[], int m);
extern Bool32 procSetNextAcc (Bool32 wrap);

//extern void procBindData (const HostBuff * const pHB, const ParamVal * const pP, const ImgOrg * const pO, const U32 iS);
extern void procFrameStat (BlockStat * const pS, const Scalar * const pAB, const ImgOrg * const pO);

//extern U32 proc2I1A (Scalar * restrict pR, Scalar * restrict pS, const ImgOrg * pO, const ParamVal * pP, const U32 iM);
extern U32 procNI (Scalar * restrict pR, Scalar * restrict pS, const ImgOrg * pO, const ParamVal * pP, const U32 nI);

#endif // PROC_H
