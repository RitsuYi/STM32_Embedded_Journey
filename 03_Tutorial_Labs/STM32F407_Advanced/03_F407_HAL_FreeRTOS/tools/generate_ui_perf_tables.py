"""H7.1静态渐变预计算（只用Python标准库，无第三方依赖）。

保持设计的8位颜色，再按固定8×8阈值抖成565。每行用重复的8像素小块压缩，
解码可从任意行/列开始，既不展开整图RAM，也不在F407上反复做平方和除法。
修改主题渐变后运行本脚本；输出是应用层查表数据，未修改LVGL核心。
"""
from pathlib import Path

BAYER = ((0,48,12,60,3,51,15,63),(32,16,44,28,35,19,47,31),
         (8,56,4,52,11,59,7,55),(40,24,36,20,43,27,39,23),
         (2,50,14,62,1,49,13,61),(34,18,46,30,33,17,45,29),
         (10,58,6,54,9,57,5,53),(42,26,38,22,41,25,37,21))


def quant(rgb, x, y):
    threshold = BAYER[y & 7][x & 7]
    def level(value, maximum):
        scaled = value * maximum
        return scaled // 255 + (scaled % 255 * 64 > threshold * 255 + 127)
    r, g, b = rgb
    return level(r,31) << 11 | level(g,63) << 5 | level(b,31)


def plot(x, y, width):
    top = (231-y)**2 * 256 // (231*231)
    bottom = y*y * 256 // (231*231)
    edge = abs(2*x-width+1)*256//width
    return (205+(top*38+bottom*10+edge*2)//256,
            222+(top*23+bottom*7+edge*2)//256,
            236+(top*15+bottom*4+edge)//256)


def ramp(top, bottom, height, y):
    # 与LVGL9.6两端点垂直渐变的frac和整数除255规则一致。
    last = 255 * height >> 8
    mix = 255 if y >= last else y*255//last
    def udiv255(v):
        return (v*0x8081) >> 23
    return tuple(udiv255(((bottom>>shift)&255)*mix+((top>>shift)&255)*(255-mix))
                 for shift in (16,8,0))


def generate():
    # x/y是原有画面的绝对坐标，预计算抖色与条带高度、裁剪起点无关。
    specs = [(292,232,0,0,8,36),(278,232,0,0,8,36),
             (480,30,0xFFFFFF,0xFAF9F6,0,0),
             (168,237,0xFFFFFF,0xFCFBF8,307,35),
             (183,237,0xFFFFFF,0xFCFBF8,292,35),
             (158,44,0xFFFFFF,0xFBFBF9,312,67),
             (174,39,0xF2FCF7,0xFFFFFF,296,94),
             (174,39,0xFFF3F5,0xFFFFFF,296,94),
             (480,46,0xFCFBF9,0xF5F5F1,0,274),
             (131,36,0xFFFFFF,0xFBFBF8,18,276),
             (150,36,0xB6E0FF,0x98CBFA,165,276),
             (131,36,0xCAE6FA,0xA9D2F0,18,276),
             (150,36,0xCAE6FA,0xA9D2F0,165,276),
             # 进度条原生渐变沿完整宽度展开，再按进度裁剪；不随进度重新拉伸。
             # 固定绝对坐标让同一像素在不同条带/局部刷新时使用相同抖色相位。
             (117,8,0xA0D8FF,0x98CBFA,319,219)]
    tiles, tile_ids, data = [], {}, []
    for width,height,top,bottom,ox,oy in specs:
        rows, runs = [], []
        for y in range(height):
            rows.append(len(runs))
            previous = None
            for block in range((width+7)//8):
                tile = tuple(quant(plot(min(block*8+i,width-1),y,width) if top==0
                                   else ramp(top,bottom,width,min(block*8+i,width-1)) if height==8
                                   else ramp(top,bottom,height,y),ox+block*8+i,oy+y)
                             for i in range(8))
                if tile not in tile_ids:
                    tile_ids[tile]=len(tiles);tiles.append(tile)
                index=tile_ids[tile]
                if index==previous:
                    runs[-1]=(block+1,index)
                else:
                    runs.append((block+1,index));previous=index
        rows.append(len(runs));data.append((rows,runs))
    lines=['/* 由tools/generate_ui_perf_tables.py生成；常量放Flash，勿手改。 */',
           'static const uint16_t ui_perf_tiles[][8] = {']
    lines += ['    {'+','.join(f'0x{v:04X}' for v in tile)+'},' for tile in tiles]
    lines.append('};')
    for index,(rows,runs) in enumerate(data):
        lines.append(f'static const uint16_t ui_perf_rows_{index}[] = {{'+','.join(map(str,rows))+'};')
        lines.append(f'static const UiPerfRun ui_perf_runs_{index}[] = {{')
        lines += ['    '+','.join('{%d,%d}'%run for run in runs[i:i+12])+',' for i in range(0,len(runs),12)]
        lines.append('};')
    lines.append('static const UiPerfImage ui_perf_images[] = {')
    for i,(w,h,top,bottom,ox,oy) in enumerate(specs):
        lines.append('    {{.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,'
                     f'.flags=LV_IMAGE_FLAGS_USER2,.w={w},.h={h},.stride={w*2}'
                     '},.data_size=sizeof(ui_perf_tiles),.data=(const uint8_t *)ui_perf_tiles},'
                     f'ui_perf_rows_{i},ui_perf_runs_{i},0x{top:06X},0x{bottom:06X}'+'},')
    lines.append('};')
    target=Path(__file__).resolve().parents[1]/'User/GUI/assets/ui_perf_tables.inc'
    target.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    size=len(tiles)*16+sum(len(rows)*2+len(runs)*4 for rows,runs in data)
    print(f'Precomputed {len(specs)} gradients: {len(tiles)} shared tiles, {size} table bytes plus descriptors')


if __name__=='__main__':
    generate()
