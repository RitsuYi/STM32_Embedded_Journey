"""历史Pillow字体生成器，供2026-10-18皮肤复现；当前屏幕用generate_screen_fonts.py。

本脚本会生成旧版ui_font_16_bold/26_bold；当前字体请用官方转换器版本重建。
为历史新增UI生成缺失汉字的4bpp字体，原字体保留为fallback（缺字时回退）。

字体使用已安装且随工程保留OFL许可的Noto Sans SC；不下载字体。
只提取界面源码中的汉字/符号，不把注释里的所有汉字打包进Flash。
"""
from pathlib import Path
import re
import argparse
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]


def generate(size, bold, chars, regular, heavy, *, name_override=None, weight=None, complete=False, x_scale=1.0):
    suffix = f"{size}" + ("_bold" if bold else "")
    base = "measurement_font_" + (suffix if size in (12,14,16) else "12")
    name = name_override or "dashboard_font_" + suffix
    original = (ROOT / f"User/GUI/fonts/{base}.c").read_text(encoding="utf-8")
    present = {int(v, 16) for v in re.findall(r"U\+([0-9A-F]+)", original)}
    missing = sorted(ord(c) for c in chars if complete or size == 10 or ord(c) not in present)
    # 拒绝空字符表；未命中此字体时由原字体渲染ASCII/已有汉字。
    font = ImageFont.truetype("C:/Windows/Fonts/NotoSansSC-VF.ttf" if weight else heavy if bold else regular, size)
    if weight:
        # 参考图标题/数字更厚；使用字体自身Weight轴，避免描边伪粗破坏字形。
        font.set_variation_by_axes([weight])
    pixels = bytearray()
    glyphs = ["    {0}, /* 字形0用于表示未找到 */"]
    for cp in missing:
        ch = chr(cp)
        x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
        w, h = x1 - x0, y1 - y0
        im = Image.new("L", (max(1, w), max(1, h)))
        ImageDraw.Draw(im).text((-x0, -y0), ch, font=font, fill=255, anchor="ls")
        if x_scale != 1.0 and w:
            # 数字采用窄体比例，保持参考图的高度和字重，不再挤占单位列。
            w=round(w*x_scale)
            im=im.resize((w,max(1,h)),Image.Resampling.LANCZOS)
            x0=round(x0*x_scale)
        values = [(v * 15 + 127) // 255 for v in im.tobytes()] if w and h else []
        offset = len(pixels)
        # LVGL无压缩4bpp按整个字形连续打包，高半字节为前一个像素。
        for i in range(0, len(values), 2):
            pixels.append(values[i] << 4 | (values[i + 1] if i + 1 < len(values) else 0))
        glyphs.append(f"    {{.bitmap_index={offset}, .adv_w={round(font.getlength(ch)*x_scale*16)}, "
                      f".box_w={w}, .box_h={h}, .ofs_x={x0}, .ofs_y={-y1}}}, /* U+{cp:04X} */")
    start = missing[0]
    out = ["/* 自动生成的新增字形；Noto Sans SC，SIL OFL 1.1，版权/许可见OFL.txt。 */",
           '#include "measurement_fonts.h"', "static const uint8_t bitmap[] = {"]
    for i in range(0, len(pixels), 20):
        out.append("    " + ",".join(f"0x{x:02x}" for x in pixels[i:i+20]) + ",")
    out += ["};", "static const lv_font_fmt_txt_glyph_dsc_t glyphs[] = {", *glyphs, "};",
            "static const uint16_t unicode_list[] = {" + ",".join(str(c-start) for c in missing) + "};",
            "static const lv_font_fmt_txt_cmap_t cmap[] = {{",
            f"    .range_start={start}, .range_length={missing[-1]-start+1}, .glyph_id_start=1,",
            f"    .unicode_list=unicode_list, .list_length={len(missing)}, .type=LV_FONT_FMT_TXT_CMAP_SPARSE_TINY",
            "}};", "static const lv_font_fmt_txt_dsc_t descriptor = {",
            "    .glyph_bitmap=bitmap,.glyph_dsc=glyphs,.cmaps=cmap,.cmap_num=1,",
            "    .bpp=4,.bitmap_format=LV_FONT_FMT_TXT_PLAIN,.stride=0", "};",
            f"const lv_font_t {name} = {{",
            "    .get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,.get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,",
            f"    .line_height={size+3 if complete else size+4},.base_line=3,.static_bitmap=1,.dsc=&descriptor" +
            ("" if complete else f",.fallback=&{base}"), "};"]
    (ROOT / f"User/GUI/fonts/{name}.c").write_text("\n".join(out)+"\n", encoding="utf-8")
    print(f"{name}: {len(missing)}新增字形，{len(pixels)}位图字节")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--regular", default="C:/Windows/Fonts/Noto Sans SC (TrueType).otf")
    ap.add_argument("--bold", default="C:/Windows/Fonts/Noto Sans SC Bold (TrueType).otf")
    args = ap.parse_args()
    chars = set()
    for relative in ["instrument/dashboard_ui.c", "measurement/measurement_page.c", "measurement/geometry_view.c"]:
        source = (ROOT / "User/GUI" / relative).read_text(encoding="utf-8")
        source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
        for literal in re.findall(r'"(?:\\.|[^"\\])*"', source):
            chars.update(c for c in literal if ord(c) >= 127 and c not in "\\\n\r")
    for size, bold in [(12, False), (14, False), (16, True)]:
        generate(size, bold, chars, args.regular, args.bold)
    generate(10, False, set("0123456789.#:%-编号时间外径内径长宽合格超差暂无检测记录"), args.regular, args.bold)
    # 主页面专用完整字形，严格区分小号说明、粗体标题与重字重大数字。
    # 既有二级页字体保留，便于只调整主界面的参考图视觉。
    complete_chars=chars|set(chr(c) for c in range(32,127))
    for size, weight, name in [(9,500,"ui_font_9"),(11,500,"ui_font_11"),
                               (13,700,"ui_font_13_bold"),(16,800,"ui_font_16_bold")]:
        # 16px只承担统计数值和判定；完整汉字集保留在11/13px，避免重复占用Flash。
        selected=(set(chr(c) for c in range(32,127))|set("合格不合格待检测")) if size==16 else complete_chars
        generate(size,True,selected,args.regular,args.bold,name_override=name,weight=weight,complete=True)
    generate(26,True,set("0123456789.-%"),args.regular,args.bold,
             name_override="ui_font_26_bold",weight=850,complete=True,x_scale=0.84)
    # 质检结果栏较窄，数字使用窄体子集，为标签与mm单位留出互不重叠的列。
    generate(13,True,set("0123456789.-"),args.regular,args.bold,
             name_override="ui_font_13_numeric",weight=800,complete=True,x_scale=0.84)
