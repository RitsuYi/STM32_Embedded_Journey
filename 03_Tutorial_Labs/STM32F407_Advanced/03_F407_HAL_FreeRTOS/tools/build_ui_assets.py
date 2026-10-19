"""将已生成的材质转换为LVGL 9.6只读资源，并生成柔和的低分辨率预览背景。

这是工程资产的缩放/格式打包步骤，不是生成新的界面效果图。
圆环保留ARGB8888精度以便缩放/混合后统一抖色；历史背景仍为RGB565。
输出直接编入Flash，不在普通SRAM展开整张图片。
"""
from pathlib import Path
import math
import random
from PIL import Image, ImageDraw, ImageFilter

ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'User/GUI/assets'


def pack(name,im,alpha):
    im=im.convert('RGBA');w,h=im.size;rgb=bytearray()
    flat=im.tobytes()
    for i in range(0,len(flat),4):
        r,g,b,a=flat[i:i+4]
        if alpha:
            # LVGL的ARGB8888在小端F407内存中按B/G/R/A排列，保留原始8位颜色。
            rgb.extend((b,g,r,a))
        else:
            pixel=((r*31+127)//255<<11)|((g*63+127)//255<<5)|((b*31+127)//255)
            rgb.extend((pixel&255,pixel>>8))
    data=rgb
    color_format='ARGB8888' if alpha else 'RGB565'
    out=['/* 由tools/build_ui_assets.py生成，只读Flash资源；禁止放入可写帧缓冲。 */',
         '#include "ui_assets.h"',f'static const uint8_t pixels[{len(data)}] = {{']
    for i in range(0,len(data),24):out.append('    '+','.join(f'0x{x:02x}' for x in data[i:i+24])+',')
    out+=['};',f'const lv_image_dsc_t {name} = {{',
          f'    .header={{.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_{color_format},.w={w},.h={h},.stride={w*(4 if alpha else 2)}}},',
          f'    .data_size={len(data)},.data=pixels','};']
    (ASSETS/(name+'.c')).write_text('\n'.join(out)+'\n',encoding='utf-8')
    print(f'{name}: {w}x{h}, {len(data)} bytes')


def pack_shadow():
    # 大圆形阴影若实时计算，LVGL会临时申请约30KB，超过现有池的连续余量。
    # 预先存成96×96的A8透明度图，只占9216B Flash，运行时按小条带混合。
    im=Image.new('L',(96,96));ImageDraw.Draw(im).ellipse((8,8,87,87),fill=45)
    im=im.filter(ImageFilter.GaussianBlur(5));data=im.tobytes()
    out=['/* 预计算柔影；A8透明度，不复制为RAM整图。 */','#include "ui_assets.h"','static const uint8_t pixels[] = {']
    for i in range(0,len(data),24):out.append('    '+','.join(f'0x{x:02x}' for x in data[i:i+24])+',')
    out+=['};','const lv_image_dsc_t ui_metal_shadow = {',
          '    .header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_A8,.w=96,.h=96,.stride=96},',
          '    .data_size=9216,.data=pixels','};']
    (ASSETS/'ui_metal_shadow.c').write_text('\n'.join(out)+'\n',encoding='utf-8')


if __name__=='__main__':
    # 用户明确允许直接提取参考图贴图；圆环保持二维外观，不再额外生成立体材质。
    reference=Image.open(ASSETS/'reference_measurement.png').convert('RGBA')
    face=reference.crop((236,263,747,774)).resize((168,168),Image.Resampling.LANCZOS)
    ring=Image.new('RGBA',(192,192));ring.paste(face,(12,12))
    # 径向透明蒙版只去掉工件外部与孔内背景，边缘保留一像素抗锯齿过渡。
    for y in range(192):
        for x in range(192):
            r=math.hypot(x-95.5,y-95.5)
            a=round(255*max(0,min(1,84-r))*max(0,min(1,r-47)))
            pixel=ring.getpixel((x,y));ring.putpixel((x,y),(*pixel[:3],a))
    pack('ui_metal_ring',ring,True)
    ring.save(ASSETS/'metal_ring_192.png')
    pack_shadow()
    # 背景以146x116生成，运行时放大2倍；细网格另由代码绘制，可在设置中关闭。
    # 中间偏暗、四周柔光，与参考图的雾蓝色摄影背景一致。
    w,h=146,116;bg=Image.new('RGB',(w,h));random.seed(407)
    for y in range(h):
        for x in range(w):
            top=math.exp(-y/19)
            bottom=(y/115)**2
            side=abs(x-73)/73*math.exp(-y/28)
            corner=math.exp(-((min(x,145-x)/24)**2+(y/18)**2))*20
            noise=0 # RGB565色深有限，不加随机噪声，避免平滑背景出现颗粒。
            rgb=tuple(round(c+top*t+bottom*b+side*s+corner+noise)
                      for c,t,b,s in [(149,57,26,13),(174,54,24,12),(199,51,22,11)])
            bg.putpixel((x,y),tuple(max(0,min(255,c)) for c in rgb))
    pack('ui_plot_background',bg,False);bg.save(ASSETS/'plot_background.png')
