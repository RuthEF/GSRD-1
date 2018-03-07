

#include "data.h"


/***/

void initOrg (ImgOrg * const pO, U16 w, U16 h, U8 flags)
{
   if (pO)
   {
      pO->def.x= w;
      pO->def.y= h;
      if (0 == (flags & 1))
      {  // planar
         pO->stride[0]= 1;     // next element, same phase
         pO->stride[3]= w * h; // same element, next phase
      }
      else
      {  // interleaved
         pO->stride[0]= 2; // same phase
         pO->stride[3]= 1; // next phase
      }
      pO->stride[1]= w * pO->stride[0]; // line
      pO->stride[2]= h * pO->stride[1]; // plane
      pO->n= 2 * pO->stride[2]; // complete buffer
   }
} // initOrg

size_t paramBytes (U16 w, U16 h) { return(MAX(w,h) * 3 * sizeof(Scalar)); }

size_t initParam (ParamVal * const pP, void *p, const Scalar kL[3], const V2U32 *pD, Scalar varR, Scalar varD) // ParamArgs *
{
   U16 i, n;
   
   // Set diffusion weights
   for ( i= 0; i<3; ++i ) { pP->kL.a[i]= kL[i] * KLA0; pP->kL.b[i]= kL[i] * KLB0; }
   pP->kRR= KR0;
   pP->kRA= KRA0;
   pP->kDB= KDB0;
   pP->pKRR= pP->pKRA= pP->pKDB= p;
   if (p)
   {
      Scalar kRR=KR0, kRA=KRA0, kDB=KDB0;
      Scalar dKRR=0, dKRA=0, dKDB=0;
      Scalar *pA= p;
      pP->n= n= MAX(pD->x, pD->y);
      dKRR= (kRR * varR) / n;
      dKDB= (kDB * varD) / n;
      pP->pKRA+= n;
      pP->pKDB+= 2*n;
      for (i= 0; i<n; ++i)
      {
         pA[i]= kRR; kRR+= dKRR;
         pA[n+i]= kRA; kRA+= dKRA;
         pA[2*n+i]= kDB; kDB+= dKDB;
      }
      return(3*pP->n);
   }
   return(0);
} // initParam

size_t initBuff (const HostBuff *pB, const V2U32 d, const U16 step)
{
   const size_t n= d.x * d.y;
   size_t  i, nB= 0;
   U16 x, y;
   
   for ( i= 0; i < n; ++i ) { pB->pAB[0][i]= 1.0; pB->pAB[0][n+i]= 0.0; }
   for ( y= step; y < d.y; y+= step )
   {
      for ( x= step; x < d.x; x+= step )
      {
         i= y * d.x + x;
         if (i < n) { pB->pAB[0][n+i]= 1.0; nB++; }
      }
   }
   return(nB);
} // initBuff


void initFS (FieldStat * const pFS, const Scalar * const pS)
{
   if (pFS)
   {
      Scalar s= 0; // or NaN ?
      if (pS) { s= *pS; }
      pFS->min= pFS->max= pFS->sum1= s;
      pFS->sum2= s * s;
   }
} // initFS

void printFS (const char *pHdr, const FieldStat * const pFS, const char *pFtr)
{
   if (pHdr && pHdr[0]) { printf("%s", pHdr); }
   if (pFS)
   {  const char *fsFmtStr= "%G, %G, %G, %G";
      if (sizeof(SSStat) > sizeof(double)) { fsFmtStr= "%G, %G, %LG, %LG"; }
      printf(fsFmtStr, pFS->min, pFS->max, pFS->sum1, pFS->sum2);
   }
   if (pFtr && pFtr[0]) { printf("%s", pFtr); }
} // printFS


