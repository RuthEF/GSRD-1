#!/usr/bin/env python

import numpy
#import scipy
#from scipy import stats
#from scipy import ndimage
from PIL import Image

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

