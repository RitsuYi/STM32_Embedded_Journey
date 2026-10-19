"""使用官方lv_font_conv为RGB565屏幕生成原尺寸字形和完整图标子集。

转换器固定为1.5.3，安装在build/FontTools；固件构建只需要生成的C文件。
4bpp表示每个字形像素有16档透明度；不横向压缩，也不启用RGB子像素排列。
"""
from pathlib import Path
import argparse
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "User/GUI/fonts"


def convert(node, converter, name, size, font, symbols):
    # 参数使用独立列表传给进程，中文字符和带空格的字体路径不会被shell拆开。
    target = OUT / (name + ".c")
    subprocess.run([node, str(converter), "--size", str(size), "--bpp", "4",
                    "--format", "lvgl", "--no-compress", "--no-prefilter",
                    "--font", str(font), "--symbols", "".join(sorted(symbols)),
                    "--autohint-strong", "--lv-include", "lvgl/lvgl.h",
                    "--lv-font-name", name, "-o", str(target)], check=True, cwd=ROOT)
    source = target.read_text(encoding="utf-8")
    # 官方旧版输出兼容LVGL9.6；显式标记只读位图，允许LVGL直接访问Flash字形。
    source = source.replace(".subpx = LV_FONT_SUBPX_NONE,",
                            ".static_bitmap = 1, /* 无压缩只读字形，不在RAM常驻整份字体。 */\n"
                            "    .subpx = LV_FONT_SUBPX_NONE,")
    # 原创页脚轮廓与第三方字体来源分别标明，避免给原创图标附上错误的来源说明。
    origin = (" * 原创页脚轮廓来自tools/build_ui_icon_font.cjs。 */\n" if name == "ui_footer_icons_16" else
              " * Noto Sans SC或Font Awesome的来源/许可见本目录README及source/。 */\n")
    source = ("/* 屏幕原尺寸字形；官方lv_font_conv 1.5.3，4bpp、强网格微调、无缩放。\n" + origin + source)
    target.write_text(source, encoding="utf-8")
    print(name, "pixels=", size, "glyphs=", len(symbols))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--node", default="node")
    parser.add_argument("--converter", type=Path,
                        default=ROOT / "build/FontTools/node_modules/lv_font_conv/lv_font_conv.js")
    parser.add_argument("--regular", type=Path, default=Path("C:/Windows/Fonts/Noto Sans SC (TrueType).otf"))
    parser.add_argument("--bold", type=Path, default=Path("C:/Windows/Fonts/Noto Sans SC Bold (TrueType).otf"))
    args = parser.parse_args()
    chars = set(chr(c) for c in range(32, 127))
    for relative in ["measurement/measurement_page.c", "measurement/geometry_view.c", "instrument/dashboard_ui.c"]:
        source = (ROOT / "User/GUI" / relative).read_text(encoding="utf-8")
        source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
        for literal in re.findall(r'"(?:\\.|[^"\\])*"', source):
            chars.update(c for c in literal if ord(c) >= 127 and c not in "\r\n")
    convert(args.node, args.converter, "ui_font_12", 12, args.regular, chars)
    convert(args.node, args.converter, "ui_font_14_bold", 14, args.bold, chars)
    # 16px用于主标题和统计；26px只放尺寸数字，均直接按目标尺寸取字，取消84%压缩。
    title_chars = set(chr(c) for c in range(32, 127)) | set("智能视觉测量仪合格不合格待检测")
    convert(args.node, args.converter, "ui_font_16_bold", 16, args.bold, title_chars)
    convert(args.node, args.converter, "ui_font_26_bold", 26, args.bold, set("0123456789.-%"))
    # 每个图标都有完整轮廓；两个尺寸单独取字，运行时不放大低分辨率图标。
    icons = {chr(c) for c in [0xE001, 0xF013, 0xF545, 0xF3ED, 0xF46C, 0xF080, 0xF15C,
                              0xF1EB, 0xF00E, 0xF010, 0xF065, 0xF058, 0xF057,
                              0xF140, 0xF04B, 0xF04D, 0xF054, 0xF030]}
    subprocess.run([args.node, str(ROOT / "tools/build_ui_icon_font.cjs")], check=True, cwd=ROOT)
    icon_font = ROOT / "build/FontTools/fonts/MeasurementUIIcons.ttf"
    for size in (16, 20):
        convert(args.node, args.converter, f"ui_icons_{size}", size, icon_font, icons)
    # 页脚使用独立的轻轮廓字形；统一16px方框，原尺寸取字，避免大实心图标失衡。
    convert(args.node, args.converter, "ui_footer_icons_16", 16,
            ROOT / "build/FontTools/fonts/MeasurementFooterIcons.ttf",
            {chr(c) for c in range(0xE101, 0xE106)})


if __name__ == "__main__":
    main()
