

#include "data.h"


/***/

static void initWrap (BoundaryWrap *pW, const Stride stride[4])
{
   pW->h[0]= stride[1]; pW->h[1]= stride[2] - stride[1];	// 0..3 LO, 2..5 HI
   pW->h[2]= -stride[0]; pW->h[3]= stride[0];
   pW->h[4]= stride[1] - stride[2]; pW->h[5]= -stride[1];

   pW->v[0]= stride[1] - stride[0]; pW->v[1]= stride[0];
   pW->v[2]= stride[1]; pW->v[3]= -stride[1];
   pW->v[4]= -stride[0]; pW->v[5]= stride[0] - stride[1];

   pW->c[0]= stride[1] - stride[0]; pW->c[1]= stride[0];
   pW->c[2]= stride[1]; pW->c[3]= stride[2] - stride[1];
   pW->c[4]= -stride[0]; pW->c[5]= stride[0] - stride[1];
   pW->d[0]= pW->c[0]; pW->d[1]= pW->c[1]; pW->d[4]= pW->c[4]; pW->d[5]= pW->c[5];
   pW->d[2]= -pW->c[2]; pW->d[3]= -pW->c[3];
} // initWrap

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
      initWrap(&(pO->wrap), pO->stride);
   }
} // initOrg

//size_t paramBytes (U16 w, U16 h) { return(MAX(w,h) * 3 * sizeof(Scalar)); }

size_t initParam (ParamVal * const pP, const Scalar kL[3], const V2U32 *pD, Scalar varR, Scalar varD) // ParamArgs *
{
   U32 i, n= 0;
    // Set diffusion weights
   for ( i= 0; i<3; ++i ) { pP->base.kL.a[i]= kL[i] * KLA0; pP->base.kL.b[i]= kL[i] * KLB0; }
   pP->base.kRR= KR0;
   pP->base.kRA= KRA0;
   pP->base.kDB= KDB0;

   pP->var.pK= NULL;
   if (pD)
   {
      n= MAX(pD->x, pD->y);
      pP->var.pK= malloc( 3 * n * sizeof(*(pP->var.pK)) );
   }
   if (pP->var.pK)
   {
      Scalar *pKRR, *pKRA, *pKDB;
      Scalar kRR=KR0, kRA=KRA0, kDB=KDB0;
      Scalar dKRR=0, dKRA=0, dKDB=0;
      
      pP->var.iKRR= 0; pP->var.iKRA= pP->var.iKRR + n; pP->var.iKDB= pP->var.iKRA+n;
      pKRR= pP->var.pK + pP->var.iKRR;
      pKRA= pP->var.pK + pP->var.iKRA;
      pKDB= pP->var.pK + pP->var.iKDB;

      dKRR= (kRR * varR) / n;
      dKDB= (kDB * varD) / n;
      for (i= 0; i<n; ++i)
      {
         pKRR[i]= kRR; kRR+= dKRR;
         pKRA[i]= kRA; kRA+= dKRA;
         pKDB[i]= kDB; kDB+= dKDB;
      }
   }
   else
   {
      pP->var.iKRR= pP->var.iKRA= pP->var.iKDB= 0;
   }
   return(n);
} // initParam

void releaseParam (ParamVal * const pP)
{
   if (pP && pP->var.pK) { free(pP->var.pK); pP->var.pK= NULL; }
} // releaseParam

Bool32 initHBT (HostBuffTab * const pT, const size_t fb, const U32 mF)
{
   U32 nF= 0, i= 0;
   Bool32 a;
   
   memset(pT, 0, sizeof(*pT));
   if (mF >= 5)
   { 
	   pT->pC= malloc(fb / 2);
	   nF+= (NULL != pT->pC);
   }
   do
   {
      pT->hfb[i].pAB= malloc(fb);
      a= (NULL != pT->hfb[i].pAB);
      if (a)
      {
		 BlockStat *pS= &(pT->hfb[i].s);
		 memset(pS, -1, sizeof(*pS));
		 pT->hfb[i].iter= -1;
         nF+= 2;
      }
   } while (a && (nF < mF) && (++i < HFB_MAX));
   return(mF == nF);
} // initHBT

void releaseHBT (HostBuffTab * const pT)
{
   for ( int i= 0; i< HFB_MAX; ++i )
   { 
      if (pT->hfb[i].pAB) { free(pT->hfb[i].pAB); pT->hfb[i].pAB= NULL; }
   }
   if (pT->pC) { free(pT->pC); pT->pC= NULL; }
} // releaseHBT


size_t initHFB (HostFB * const pB, const V2U32 d, const U16 step)
{
   const size_t n= d.x * d.y;
   size_t  i, nB= 0;
   U16 x, y;
   // HACKY: assumes planar org.
   for ( i= 0; i < n; ++i ) { pB->pAB[i]= 1.0; pB->pAB[n+i]= 0.0; }
   for ( y= step; y < d.y; y+= step )
   {
      for ( x= step; x < d.x; x+= step )
      {
         i= y * d.x + x;
         if (i < n) { pB->pAB[n+i]= 1.0; nB++; }
      }
   }
   pB->iter= 0;
   // stat ?
   return(nB);
} // initHFB


void initFS (FieldStat * const pFS, const Scalar * const pS)
{
   if (pFS)
   {
      Scalar s= 0; // or NaN ?
      if (pS) { s= *pS; }
      pFS->min= pFS->max= s;

      pFS->s.m[0]= (NULL != pS);
      pFS->s.m[1]= s;
      pFS->s.m[1]= s * s;
      //pFS->sum1= s;
      //pFS->sum2= s * s;
   }
} // initFS

void printFS (const char *pHdr, const FieldStat * const pFS, const char *pFtr)
{
   if (pHdr && pHdr[0]) { printf("%s", pHdr); }
   if (pFS)
   {
      StatRes1 r;

      printf("%G\t%G", pFS->min, pFS->max);

      //if (sizeof(SMVal) > sizeof(double)) { fsFmtStr= "%LG, %LG"; }
      if ( statGetRes1(&r, &(pFS->s), 0) )
      {
         printf("\t%G\t%G\t%G", pFS->s.m[1], r.m, r.v); //sqrtf(r.v));
      }
   }
   if (pFtr && pFtr[0]) { printf("%s", pFtr); }
} // printFS
