#!/usr/bin/env python

import sys
import time
import numpy
import scipy
from scipy import stats
from scipy import ndimage
from PIL import Image
# from matplotlib import pyplot
# from gsrdutil import saveImage, saveBuff

G_IMG_LEN= 512
gImgDef= (G_IMG_LEN, G_IMG_LEN)
gImgDefRGB= (G_IMG_LEN, G_IMG_LEN, 3)

gTileDef= (32,32)
gTileBorder= (4,4)

# Laplacian coefficients
L_K2= 0.05
L_K1= 0.2
L_K0= -4 * (L_K1 + L_K2)

# Laplacian convolution kernel
kL9P= numpy.array([[L_K2,L_K1,L_K2],[L_K1,L_K0,L_K1],[L_K2,L_K1,L_K2]])

def laplace9P (a, r):
    return ndimage.convolve(a, r * kL9P, mode='wrap', cval= 0.0)

def saveRGB(path,ina,nI,ext):
    name= "gsrd{0:05d}".format(nI)
    img= Image.fromarray(ina)
    img.save(path+name+ext)

def saveImage(a, b, mma, mmb, nI):
    _rgb= numpy.zeros(gImgDefRGB, dtype=numpy.uint8, order='C')
    if mma[1] > 0:
        _sa= 255.0 / mma[1]
    if mmb[1] > 0:
        _sb= 255.0 / mmb[1]
    _rgb[... ,0]= _sa * a
    _rgb[... ,2]= _sb * b
    #img[0:7, 0:3, :]= 255 # orientation marker
    saveRGB("png/", _rgb, nI, ".png")

def saveBuff(a,b,nI):
    #_ab= numpy.stack((a,b),axis=0)
    #_t= ( _ab.shape[2], _ab.shape[1], _ab.shape[0] )
    # numpy.stack() unavailable on Centos7 due to decrepit python/numpy/scipy versions required by <yum>
    _ab= numpy.concatenate( (a,b) )
    _t = ( a.shape[1], a.shape[0], 2 )
    print _t
    _s= str( _t ).replace(" ", "")
    _pathname= "raw/gsrd{0:05d}{1}F64.raw".format(nI,_s)
    print _pathname
    _ab.tofile(_pathname)

def output (a, b, nI):
    sa= scipy.stats.describe(a.reshape(-1))
    sb= scipy.stats.describe(b.reshape(-1))
    print "--------"
    print "iter=", nI
    print "stats: a=", sa
    print "stats: b=", sb
    saveBuff(a, b, nI)
    #saveImage(a, b, sa.minmax, sb.minmax, 0)
    print "mm:", sa[1], sb[1]
    saveImage(a, b, sa[1], sb[1], 0)

#def showFrame(ina):
    #pyplot.imshow(ina)
    #pyplot.show()

def seedRandomPoints (arr, numPts):
    for _n in xrange(numPts):
        _i= numpy.random.randint(0,gImgDef[0]-1)
        _j= numpy.random.randint(0,gImgDef[1]-1)
        arr[_i,_j]= 1.0

def seedTileRandomPoints (arr):
    _nn= ( gImgDef[0] / gTileDef[0], gImgDef[1] / gTileDef[1] )
    _rr= ( gTileDef[0] - (1 + 2 * gTileBorder[0]), gTileDef[1] - (1 + 2 * gTileBorder[1]) )
    #print(_nn, _rr)
    for _i in xrange(_nn[0]):
        _i0= _i * gTileDef[0] + gTileBorder[0]
        for _j in xrange(_nn[1]):
            _j0= _j * gTileDef[1] + gTileBorder[1]
            #print(_i0, _j0)
            _i= _i0 + numpy.random.randint(0, _rr[0])
            _j= _j0 + numpy.random.randint(0, _rr[1])
            arr[_i,_j]= 1.0

def seedSpacedRandomPoints (arr,sep,randr):
    _nn= ( (gImgDef[0] / sep)-1, (gImgDef[1] / sep)-1 )
    nP= 0
    #print(_nn, _rr)
    for _i in xrange(_nn[0]):
        _i0= (_i+1) * sep
        for _j in xrange(_nn[1]):
            _j0= (_j+1) * sep
            #print(_i0, _j0)
            _i= _i0 + numpy.random.randint(-randr, randr)
            if _i >= 0 and _i < gImgDef[0]:
                _j= _j0 + numpy.random.randint(-randr, randr)
                if _j >= 0 and _j < gImgDef[1]:
                    arr[_i,_j]= 1.0
                    nP+= 1
    return nP

def genRateMaps ():
    mKRA= numpy.zeros(gImgDef, dtype=float, order='C')
    mKDB= numpy.zeros(gImgDef, dtype=float, order='C')
    for _i in xrange(gImgDef[0]):
        kra= KRA0 + ((DKRA0 * _i) / gImgDef[0])
        for _j in xrange(gImgDef[1]):
            kdb= KDB0 + ((DKDB0 * _j) / gImgDef[1])
            mKRA[_i,_j]= kra
            mKDB[_j,_i]= kdb
    return mKRA, mKDB

def genAB ():
    # Initial states of reagents
    a= numpy.ones(gImgDef, dtype=float, order='C')
    b= numpy.zeros(gImgDef, dtype=float, order='C')
    # Seed reagent B sparsely
    # seedRandomPoints(b,100)
    # seedTileRandomPoints(b)
    seedSpacedRandomPoints(b,32,16)
    return a, b

# Rate constants
#KT=  1.0
KR0=   0.125	# Reaction
KRA0=  0.0115	# Replenishment A
DKRA0= 0.0	    # KRA0 / 4.0	# Var rep A
KDB0=  0.0195	# Decay B
DKDB0= 0.0	    # KDB0 / 4.0	# Var dec B
KLA0=  0.25		# Diffusion A
KLB0=  0.025	# Diffusion B

try:
    nI= 0
    maxIter= 1000
    iterStep= 100

    a, b= genAB()
    output(a, b, nI)

    while (nI < maxIter):
        for i in xrange(iterStep):
            _rab2= KR0 * a * b * b
            a+= laplace9P(a, KLA0) - _rab2 + KRA0 * (1 - a)
            b+= laplace9P(b, KLB0) + _rab2 - KDB0 * b
            nI+= 1
        output(a, b, nI)

except KeyboardInterrupt:
    print("\nbye...\n")
    pass


