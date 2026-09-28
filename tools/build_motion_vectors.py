"""Analyze source sprite motion. Emits vector metadata only; never edits or writes raster artwork."""
from pathlib import Path
import json, struct, sys
import cv2
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parent.parent
meta=json.loads((root/'assets/sprite-metadata.json').read_text())
sources={m['asset']:Image.open(root/'assets'/m['asset']).convert('RGBA') for m in meta}
gray=[]
for m in meta:
    r=m['ratio']; crop=sources[m['asset']].crop(m['rect']).convert('RGBa')
    baked=crop.transform((256,256),Image.Transform.AFFINE,(1/r,0,m['pivot_x']-128/r,0,1/r,m['pivot_y']-244/r),Image.Resampling.BICUBIC)
    a=np.array(baked,dtype=np.float32); a[:,:,:3]=np.minimum(a[:,:,:3],a[:,:,3,None])
    rgb=np.uint8(np.clip(a[:,:,:3]+(255-a[:,:,3,None])*.94,0,255))
    gray.append(cv2.cvtColor(rgb,cv2.COLOR_RGB2GRAY))
pairs=[tuple(map(int,line.split())) for line in Path(sys.argv[1]).read_text().splitlines()]
grid=np.linspace(0,255,33,dtype=np.float32); xx,yy=np.meshgrid(grid,grid)
dis=cv2.DISOpticalFlow_create(cv2.DISOPTICAL_FLOW_PRESET_MEDIUM)
dis.setVariationalRefinementIterations(12)
cv2.setNumThreads(2)
payload=bytearray(b'LILIFLOW'+struct.pack('<II',len(pairs),33))
for a,b in pairs:
    f=dis.calc(gray[a],gray[b],None); backward=dis.calc(gray[b],gray[a],None)
    coarse=np.concatenate([cv2.remap(f,xx,yy,cv2.INTER_LINEAR),cv2.remap(backward,xx,yy,cv2.INTER_LINEAR)],axis=2)
    quantized=np.int16(np.rint(np.clip(coarse,-100,100)*64))
    payload+=struct.pack('<HH',a,b)+quantized.astype('<i2').tobytes()
destination=root/'assets/motion-vectors.bin';destination.write_bytes(payload)
print(f'Computed motion-vector metadata for {len(pairs)} frame pairs: {len(payload)} bytes. Source images unchanged.')
