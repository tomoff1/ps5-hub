"""Génère l'icône géométrique originale PS5 Hub (Pillow)."""
from pathlib import Path
from PIL import Image, ImageDraw

root=Path(__file__).resolve().parent.parent
im=Image.new('RGB',(1024,1024));p=im.load()
for y in range(1024):
    for x in range(1024):
        glow=max(0,1-((x-800)**2+(y-180)**2)**0.5/1000)
        p[x,y]=(int(9+12*glow),int(17+34*glow),int(30+42*glow))
d=ImageDraw.Draw(im)
d.rounded_rectangle((180,180,844,844),radius=170,fill=(18,39,53),outline=(82,134,145),width=3)
d.rounded_rectangle((300,310,392,714),radius=28,fill=(128,237,212))
d.rounded_rectangle((632,310,724,714),radius=28,fill=(128,237,212))
d.rounded_rectangle((354,466,670,558),radius=28,fill=(128,237,212))
d.ellipse((680,230,752,302),fill=(237,249,255))
im.resize((512,512),Image.Resampling.LANCZOS).save(root/'sce_sys/icon0.png')
