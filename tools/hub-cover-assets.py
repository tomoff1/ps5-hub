"""Illustrations originales des trois accès de PS5 Hub, préparées en RGBA borné."""
from pathlib import Path
from PIL import Image,ImageDraw
import math
import struct

root=Path(__file__).resolve().parent.parent/'assets/covers'
root.mkdir(parents=True,exist_ok=True)
for index,name in enumerate(('hub-feature','hub-homebrew','hub-files')):
    image=Image.new('RGB',(768,480));pixels=image.load()
    palettes=[((8,24,44),(74,190,175)),((24,16,55),(154,123,234)),((10,30,48),(87,166,230))]
    dark,light=palettes[index]
    for y in range(480):
        for x in range(768):
            glow=max(0,1-math.hypot(x-550,y-80)/700)
            pixels[x,y]=tuple(int(dark[c]+(light[c]-dark[c])*glow*.7) for c in range(3))
    draw=ImageDraw.Draw(image)
    for radius in (180,240,310,400):draw.arc((550-radius,90-radius,550+radius,90+radius),10,330,fill=(*light,),width=2)
    if index==0:
        draw.rounded_rectangle((210,150,566,346),radius=62,fill=(18,32,53),outline=(173,249,228),width=7)
        draw.line((260,245,342,245),fill=(230,255,249),width=17);draw.line((301,204,301,286),fill=(230,255,249),width=17)
        for x,y in ((463,214),(498,253)):draw.ellipse((x-12,y-12,x+12,y+12),fill=(128,237,212))
    elif index==1:
        for x,y,w,h in ((185,200,130,126),(335,115,133,211),(488,164,104,162)):
            draw.rounded_rectangle((x,y,x+w,y+h),radius=24,fill=(47,37,82),outline=(218,201,255),width=6)
            draw.rounded_rectangle((x+20,y+22,x+w-20,y+46),radius=8,fill=(168,142,225))
    else:
        draw.rounded_rectangle((213,150,372,212),radius=18,fill=(92,163,221))
        draw.rounded_rectangle((213,184,565,344),radius=24,fill=(43,92,139),outline=(173,222,255),width=6)
        draw.line((295,257,483,257),fill=(197,234,255),width=9)
        draw.line((295,293,435,293),fill=(130,193,239),width=9)
    image=image.resize((256,160),Image.Resampling.LANCZOS).convert('RGBA')
    (root/(name+'.hubart')).write_bytes(b'HUBA'+struct.pack('<II',256,160)+image.tobytes())
