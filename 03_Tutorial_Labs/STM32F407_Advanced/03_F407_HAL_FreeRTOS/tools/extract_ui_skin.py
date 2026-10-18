"""按用户明确授权，将参考图中的二维UI素材提取为可运行的LVGL皮肤。

保留真实的字体/图标/阴影/卡片纹理；擦除参考图示例数值、日期、连接状态，
由运行代码绘制当前数据。按钮仍是独立LVGL对象，不是不可操作的整屏截图。
"""
from pathlib import Path
from PIL import Image,ImageStat
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'User/GUI/assets'
NORMAL=OUT/'reference_measurement.png'
QUALITY=OUT/'reference_inspection.png'


def clear(im,rect,sample):
    # 从同一卡片的空白边缘提取逐行底色，移除会变动的示例数据，保留圆角与阴影。
    x0,y0,x1,y1=rect
    for y in range(y0,y1):
        rgb=tuple(round(c) for c in ImageStat.Stat(im.crop((sample,y,sample+2,y+1))).mean)
        for x in range(x0,x1):im.putpixel((x,y),rgb)


def pack(name,im):
    # 每张素材独立使用256色，提高局部渐变与边缘的有效颜色精度。
    # 不加像素抖动，保持参考图大面积浅蓝/白渐变的洁净观感。
    im=im.convert('RGB');q=im.quantize(colors=256,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
    palette=q.getpalette();palette+=([0]*(768-len(palette)));data=bytearray()
    for i in range(256):
        r,g,b=palette[i*3:i*3+3];v=((r*31+127)//255<<11)|((g*63+127)//255<<5)|((b*31+127)//255)
        data.extend((v&255,v>>8))
    data.extend(q.tobytes());w,h=im.size
    lines=['/* 用户参考图提取的二维皮肤；自定义逐行解码，禁止整图展开到RAM。 */','#include "ui_skin.h"','static const uint8_t data[] = {']
    for i in range(0,len(data),24):lines.append('    '+','.join(f'0x{b:02x}' for b in data[i:i+24])+',')
    lines+=['};',f'const lv_image_dsc_t {name} = {{',f'    .header={{.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_I8,.flags=LV_IMAGE_FLAGS_USER1,.w={w},.h={h},.stride={w}}},',f'    .data_size={len(data)},.data=data','};']
    (OUT/(name+'.c')).write_text('\n'.join(lines)+'\n',encoding='utf-8');im.save(OUT/(name+'.png'))
    print(name,w,h,len(data))


if __name__=='__main__':
    n=Image.open(NORMAL).convert('RGB').resize((480,320),Image.Resampling.LANCZOS)
    q=Image.open(QUALITY).convert('RGB').resize((480,320),Image.Resampling.LANCZOS)
    header=n.copy();clear(header,(321,7,369,24),365);clear(header,(400,5,475,25),397);clear(header,(372,7,392,24),397)
    pack('skin_header',header.crop((0,0,480,30)))
    normal=n.copy()
    for rect,sample in [((406,43,470,59),402),((379,73,469,104),344),((348,80,373,99),344),
                        ((379,118,469,149),344),((348,124,373,143),344),((317,191,468,221),466),
                        ((318,244,350,266),353),((393,244,462,265),464)]:clear(normal,rect,sample)
    pack('skin_measurement',normal.crop((307,35,475,272)))
    inspection=q.copy()
    for rect,sample in [((390,72,468,85),387),((296,89,382,134),379),((418,95,468,111),387),
                        ((418,113,468,128),387),((299,179,330,202),330),((341,179,369,202),370),
                        ((381,179,405,202),407),((418,165,466,214),416),((295,249,470,274),471),
                        ((404,96,420,110),469),((404,113,420,128),469),
                        ((453,99,467,110),469),((453,116,467,128),469),
                        ((365,238,399,247),360),((401,238,438,247),440),
                        ((297,71,313,86),316)]:clear(inspection,rect,sample)
    pack('skin_inspection',inspection.crop((292,35,475,275)))
    pack('skin_pass',q.crop((303,96,377,126)))
    footer=n.copy()
    # 保留三个按钮周围的真实柔影。仅擦除会随模式变化的左键/中键内容。
    # 从同一按钮的左右空白带逐行插值，保证擦除区跟随原有纵向高光。
    for x0,x1,left,right in [(43,127,37,131),(203,291,194,298)]:
        for y in range(281,305):
            a=n.getpixel((left,y));b=n.getpixel((right,y))
            for x in range(x0,x1):
                t=(x-left)/(right-left)
                footer.putpixel((x,y),tuple(round(a[c]*(1-t)+b[c]*t) for c in range(3)))
    pack('skin_footer',footer.crop((0,276,480,320)))
    for name,rect in [('skin_button_white',(17,276,148,309)),('skin_button_blue',(165,276,315,309))]:
        button=n.crop(rect);w,h=button.size
        for y in range(3,h-2):
            left=button.getpixel((12,y));right=button.getpixel((w-13,y))
            for x in range(21,w-21):
                t=(x-12)/(w-25);button.putpixel((x,y),tuple(round(left[c]*(1-t)+right[c]*t) for c in range(3)))
        pack(name,button)
    pack('skin_zoom_in',n.crop((213,42,239,65)))
    pack('skin_zoom_out',n.crop((242,42,268,65)))
    pack('skin_zoom_fit',n.crop((269,42,295,65)))
    pack('skin_settings_icon',n.crop((372,282,398,307)))
    pack('skin_play_icon',n.crop((204,282,224,305)))
    pack('skin_ruler_icon',q.crop((51,282,75,307)))
