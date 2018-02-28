
#include "util.h"

// Replacement for strchr() (not correctly supported by pgi-cc) with extras
const char *sc (const char *s, const char c, const char * const e, const I8 o)
{
   if (s)
   {
      if (e) { while ((s <= e) && (*s != 0) && (*s != c)) { ++s; } }
      else { while ((*s != 0) && (*s != c)) { ++s; } }
      if (*s == c) { return(s+o); }
   }
   return(NULL);
} // sc

const char *stripPath (const char *path)
{
   if (path && *path)
   {
      const char *t= path, *l= path;
      do
      {
         path= t;
         while (('\\' == *t) || ('/' == *t) || (':' == *t)) { ++t; }
         if (t > path) { l= t; }
         t += (0 != *t);
      } while (*t);
      return(l);
   }
   return(NULL);
} // stripPath

size_t fileSize (const char * const path)
{
   if (path && path[0])
   {
      struct stat sb={0};
      int r= stat(path, &sb);
      if (0 == r) { return(sb.st_size); }
   }
   return(0);
} // fileSize

int scanZ (size_t * const pZ, const char s[])
{
   const char *pE=NULL;
   int n=0;
   long long int i= strtoll(s, &pE, 10);
   if (pE && (pE > s))
   {
      if (pZ) { *pZ= i; }
      n= pE - s;
   }
   return(n);
} // scanZ

int scanVI (int v[], const int vMax, ScanSeg * const pSS, const char s[])
{
   ScanSeg ss={0,0};
   const char *pT1, *pT2;
   int nI=0;
   
   pT1= sc(s, '(', NULL, 0);
   pT2= sc(pT1, ')', NULL, 0);
   if (pT1 && pT2 && isdigit(pT1[1]) && isdigit(pT2[-1]))
   {  // include delimiters in segment...
      ss.start= pT1 - s;
      ss.len= pT2 + 1 - pT1;
      // but exclude from further scan
      ++pT1; --pT2;
      do
      {
         const char *pE=NULL;
         v[nI]= strtol(pT1, &pE, 10);
         if (pE && (pE > pT1)) { nI++; pT1= pE; }
         //v[nI++]= atoi(pT1);
         pT1= sc(pT1, ',', pT2, 1);
      } while (pT1);
   }
   if (pSS) { *pSS= ss; }
   return(nI);
} // scanVI

U8 scanChZ (const char *s, char c)
{
   if (s)
   {
      c= toupper(c);
      while (*s && (toupper(*s) != c)) { ++s; }
      if (*s)
      {
         size_t z;
         if ((scanZ(&z, s+1) > 0) && (z > 0) && (z <= 64)) { return((U8)z); }
      }
   }
   return(0);
} // scanChZ

int scanDFI (DataFileInfo * pDFI, const char * const path)
{
   size_t bytes= fileSize(path);
   //FILE *hF= fopen(pCh,"r"); if (hF) { r= fread(b, 1, 8, hF); fclose(hF); }
   if (bytes > 0)
   {
      pDFI->path=  path;
      pDFI->name=  stripPath(path);
      pDFI->bytes= bytes;
      pDFI->nV=    scanVI(pDFI->v, 4, &(pDFI->vSS), pDFI->name);
      const char *p= pDFI->name + pDFI->vSS.start + pDFI->vSS.len;
      pDFI->elemBits= scanChZ(p, 'f');
      //if (0 == pDFI->elemBits) { pDFI->elemBits= 64; }
   }
   return(0);
} // scanDFI

int scanArgs (ArgInfo *pAI, const char *a[], int nA)
{
   const char *pCh, *pT1, *pT2;
   ArgInfo tmpAI;
   int n= 0;
   if (NULL == pAI) { pAI= &tmpAI; }
   while (nA-- > 0)
   {
      pCh= a[nA];
      if ('-' != pCh[0])
      {
         n+= scanDFI(&(pAI->dfi), pCh);
      }
   }
   if (0 == pAI->proc.maxIter) { pAI->proc.maxIter= 5000; }
   if (0 == pAI->proc.subIter) { pAI->proc.subIter= 1000; }
   return(n);
} // scanArgs

#ifdef UTIL_TEST

int utilTest (void)
{
   return(0);
} // utilTest

#endif

#ifdef UTIL_MAIN

int main (int argc, char *argv[])
{
   return utilTest();
} // utilTest

#endif
