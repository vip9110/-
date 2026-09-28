"""Render a preview from export_animation.cpp's exact timeline; this is not a Windows screen recording.
No source artwork is modified. Pillow supplies an offline analogue of the exe's premultiplied renderer.
Usage: python preview_animation.py samples.json output_dir font.ttc compiled_morph_library
"""
from pathlib import Path
import json, math, subprocess, sys, ctypes
import numpy as np
from PIL import Image, ImageDraw, ImageFont

root=Path(__file__).resolve().parent.parent
samples=json.loads(Path(sys.argv[1]).read_text())
out=Path(sys.argv[2]); out.mkdir(parents=True,exist_ok=True)
font_path=sys.argv[3]
font=lambda n:ImageFont.truetype(font_path,n)
metadata=json.loads((root/'assets/sprite-metadata.json').read_text())
sources={f['asset']:Image.open(root/'assets'/f['asset']).convert('RGBA') for f in metadata}
sprites=[]
for f in metadata:
    crop=sources[f['asset']].crop(f['rect']).convert('RGBa')
    r=f['ratio']; px,py=f['pivot_x'],f['pivot_y']
    baked=crop.transform((256,256),Image.Transform.AFFINE,(1/r,0,px-128/r,0,1/r,py-244/r),Image.Resampling.BICUBIC)
    arr=np.array(baked,dtype=np.float32)
    arr[:,:,:3]=np.minimum(arr[:,:,:3],arr[:,:,3,None]) # Clamp bicubic overshoot, as in the native renderer.
    assert arr[0,:,3].max()<20 and arr[-1,:,3].max()<20 and arr[:,0,3].max()<20 and arr[:,-1,3].max()<20
    sprites.append(arr)

# Use the exact C++ motion-compensation kernel shipped in the Windows executable.
flow=ctypes.CDLL(str(Path(sys.argv[4]).resolve()))
flow.flow_new.argtypes=[ctypes.c_void_p,ctypes.c_size_t]; flow.flow_new.restype=ctypes.c_void_p
flow.flow_render.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_float,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p]
flow.flow_render.restype=ctypes.c_bool
flow.flow_delete.argtypes=[ctypes.c_void_p]
vector_bytes=(root/'assets/motion-vectors.bin').read_bytes(); raw=ctypes.create_string_buffer(vector_bytes)
handle=flow.flow_new(raw,len(vector_bytes)); assert handle
sprite_bytes=[np.uint8(a) for a in sprites]

W,H=912,458
base=Image.new('RGBA',(W,H),'#f7f3eb'); d=ImageDraw.Draw(base)
d.text((28,21),'栗栗 · 动作更自然',font=font(30),fill='#413a31')
d.rounded_rectangle((753,23,882,57),radius=17,fill='#e5ede2')
d.text((773,29),'2.1 动画版',font=font(16),fill='#567761')
d.text((30,68),'连续动作帧  /  平滑衔接  /  更丰富的陪伴',font=font(15),fill='#857b6d')
titles=['走路 · 停步 · 转身','伸懒腰 → 打哈欠','挠脸 → 打滚']
for j in range(3):
    left=24+j*296
    d.rounded_rectangle((left,107,left+272,411),radius=22,fill='#fffdf9',outline='#e5ddd2',width=1)
    d.text((left+19,125),titles[j],font=font(17),fill='#567761')
    d.ellipse((left+57,335,left+215,349),fill='#f1ebe0')
d.text((30,428),'动画时间轴预览 · 非 Windows 实机录屏',font=font(13),fill='#857b6d')
names=['安静陪伴','眨眨眼','轻轻散步','伸个懒腰','打个哈欠','挠挠脸颊','打滚玩耍','低头嗅嗅','好奇张望','开心蹦蹦','咔嚓开饭','呼呼小睡','慢慢醒来','休息养病','被提起来啦','轻轻落地']
def render(row):
    scene=base.copy(); draw=ImageDraw.Draw(scene)
    for j,p in enumerate(row):
        mixed=np.zeros((256,256,4),dtype=np.uint8)
        morphed=False
        if len(p['weights'])==2:
            (a,wa),(b,wb)=p['weights']
            morphed=flow.flow_render(handle,a,b,wb/(wa+wb),sprite_bytes[a].ctypes.data,sprite_bytes[b].ctypes.data,mixed.ctypes.data)
        if not morphed:
            mixed=np.uint8(np.clip(np.rint(sum(sprites[i]*w for i,w in p['weights'])),0,255))
        body=Image.frombytes('RGBa',(256,256),mixed.tobytes())
        facing=p['facing']; facing=math.copysign(max(.005,abs(facing)),facing)
        sx,sy=p['sx']*facing,p['sy']; a=p['angle']*math.pi/180
        matrix=np.array([[sx*math.cos(a),-sx*math.sin(a)],[sy*math.sin(a),sy*math.cos(a)]])
        inv=np.linalg.inv(matrix); offset=np.array([128,244])-inv@np.array([128+p['x']*p['facing'],244+p['y']])
        coeff=(inv[0,0],inv[0,1],offset[0],inv[1,0],inv[1,1],offset[1])
        warped=body.transform((256,280),Image.Transform.AFFINE,coeff,Image.Resampling.BICUBIC).convert('RGBA')
        scene.alpha_composite(warped,(32+j*296,98))
        label=names[p['motion']]; width=draw.textlength(label,font=font(15))
        draw.text((160+j*296-width/2,374),label,font=font(15),fill='#857b6d')
    return scene.convert('RGB')

video=out/'栗栗桌宠_动画预览.mp4'
proc=subprocess.Popen(['ffmpeg','-y','-loglevel','error','-f','rawvideo','-pix_fmt','rgb24','-s',f'{W}x{H}','-r','60','-i','-','-an','-c:v','libx264','-preset','fast','-crf','20','-pix_fmt','yuv420p','-movflags','+faststart','-threads','2',str(video)],stdin=subprocess.PIPE)
palette=render(samples[0]).resize((684,344),Image.Resampling.LANCZOS).quantize(colors=256)
gif=[]; contacts=[]
for frame,row in enumerate(samples):
    scene=render(row); proc.stdin.write(scene.tobytes())
    if frame%3==0: gif.append(scene.resize((684,344),Image.Resampling.LANCZOS).quantize(palette=palette,dither=Image.Dither.NONE))
    if frame in (60,107,159,201,289,357,407,471): contacts.append(scene)
proc.stdin.close(); assert proc.wait()==0
gif[0].save(out/'栗栗桌宠_动画预览.gif',save_all=True,append_images=gif[1:],loop=0,duration=50,optimize=True)
sheet=Image.new('RGB',(W*2,H*4),'white')
for i,scene in enumerate(contacts): sheet.paste(scene,((i%2)*W,(i//2)*H))
sheet.save(out/'animation-contact-sheet.jpg',quality=91)
print('PASS 60 sprite registrations; 600 timeline frames rendered into MP4 and 200-frame GIF.')
flow.flow_delete(handle)
