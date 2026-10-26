"""按预览的微软雅黑及11/12/13/18/24px字号生成系统页子集，不改变原主页字体。"""
from pathlib import Path
import argparse
import re
import struct
import sys
# 辅助脚本的字节码不属于固件源码，避免在tools下产生__pycache__文件。
sys.dont_write_bytecode = True
from generate_screen_fonts import convert, ROOT

parser = argparse.ArgumentParser()
parser.add_argument("--node", default="node")
args = parser.parse_args()


def single_font(collection, target):
    """Windows字体集合含多个字面；将首个字面的SFNT表重打包为转换器能读取的TTF。
    表内容保持原样，仅修正偏移和校验和，输出到忽略的build目录，不改系统字体。
    """
    data = collection.read_bytes()
    offset = struct.unpack_from(">I", data, 12)[0]
    count = struct.unpack_from(">H", data, offset + 4)[0]
    result = bytearray(data[offset:offset + 12] + bytes(16 * count))
    head = None
    for i in range(count):
        tag, checksum, start, length = struct.unpack_from(">4sIII", data, offset + 12 + 16 * i)
        result.extend(bytes((-len(result)) % 4))
        new_offset = len(result)
        block = bytearray(data[start:start + length])
        if tag == b"head":
            block[8:12] = bytes(4)
            head = new_offset
        result.extend(block)
        struct.pack_into(">4sIII", result, 12 + 16 * i, tag, checksum, new_offset, length)
    result.extend(bytes((-len(result)) % 4))
    assert head is not None
    checksum = sum(struct.unpack(">" + "I" * (len(result) // 4), result)) & 0xFFFFFFFF
    struct.pack_into(">I", result, head + 8, (0xB1B0AFBA - checksum) & 0xFFFFFFFF)
    target.write_bytes(result)
    return target

source = (ROOT / "User/GUI/instrument/system_info_page.c").read_text(encoding="utf-8")
source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
chars = set(chr(c) for c in range(32, 127))
for literal in re.findall(r'"(?:\\.|[^"\\])*"', source):
    chars.update(c for c in literal if ord(c) >= 127 and c not in "\r\n")
converter = ROOT / "build/FontTools/node_modules/lv_font_conv/lv_font_conv.js"
fonts = Path("C:/Windows/Fonts")
regular = single_font(fonts / "msyh.ttc", ROOT / "build/FontTools/fonts/system-msyh.ttf")
bold = single_font(fonts / "msyhbd.ttc", ROOT / "build/FontTools/fonts/system-msyh-bold.ttf")
for name, size, font in (("system_font_11", 11, regular),
                         ("system_font_12", 12, regular),
                         ("system_font_12_bold", 12, bold),
                         ("system_font_13", 13, regular),
                         ("system_font_13_bold", 13, bold)):
    subset = set("概览任务内存设备") if size == 13 else chars
    convert(args.node, converter, name, size, font, subset)
# 大字只含数字和标点，避免为少量数值重复常驻完整汉字字库。
convert(args.node, converter, "system_font_16_bold", 16, bold, set(chr(c) for c in range(32, 127)))
numeric = set("0123456789.:%~≈-/ ")
convert(args.node, converter, "system_font_18", 18, regular, numeric)
convert(args.node, converter, "system_font_22", 22, regular, numeric)
convert(args.node, converter, "system_font_24", 24, regular, numeric)
# 更正通用转换器的字体来源注释，本页使用预览字体，原主页继续使用Noto Sans。
for target in (ROOT / "User/GUI/fonts").glob("system_font_*.c"):
    value = target.read_text(encoding="utf-8").replace(
        " * Noto Sans SC或Font Awesome的来源/许可见本目录README及source/。 */",
        " * Microsoft YaHei位图子集，来自本机Windows字体，用于还原已确认的界面预览。 */")
    target.write_text(value, encoding="utf-8")
