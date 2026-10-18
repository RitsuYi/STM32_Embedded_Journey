/* 从已保存的Font Awesome源字体取完整轮廓，并合成独立的Wi-Fi断开字形。
 * 斜杠在字体坐标中合成，随后统一取字；运行时不叠加可能被条带边界裁切的细线。 */
const fs = require('fs');
const path = require('path');
const root = path.resolve(__dirname, '..');
const opentype = require(path.join(root, 'build/FontTools/node_modules/lv_font_conv/node_modules/opentype.js'));
const source = opentype.loadSync(path.join(root, 'User/GUI/fonts/source/FontAwesome5-Solid+Brands+Regular.woff'));
const points = [0xF013,0xF545,0xF3ED,0xF46C,0xF080,0xF15C,0xF1EB,0xF00E,0xF010,
                0xF065,0xF058,0xF057,0xF140,0xF04B,0xF04D,0xF054,0xF030];
const glyphs = [new opentype.Glyph({name:'.notdef',advanceWidth:512,path:new opentype.Path()})];
for (const cp of points) {
    const glyph = source.charToGlyph(String.fromCodePoint(cp));
    if (!glyph.index) throw new Error(`缺少图标 U+${cp.toString(16)}`);
    glyphs.push(new opentype.Glyph({name:glyph.name,unicode:cp,advanceWidth:glyph.advanceWidth,path:glyph.path}));
}
const wifi = source.charToGlyph(String.fromCodePoint(0xF1EB));
const offline = new opentype.Path();
offline.commands = wifi.path.commands.map(command => ({...command}));
// 512单位对应字号的像素高度；这条斜杠约为16px字形中的1.5px宽，保持完整且可辨认。
offline.moveTo(64,416);offline.lineTo(96,448);offline.lineTo(576,-32);offline.lineTo(544,-64);offline.close();
glyphs.push(new opentype.Glyph({name:'wifi-disconnected',unicode:0xE001,advanceWidth:wifi.advanceWidth,path:offline}));
const result = new opentype.Font({familyName:'Measurement UI Icons',styleName:'Regular',
    unitsPerEm:source.unitsPerEm,ascender:source.ascender,descender:source.descender,glyphs});
const directory = path.join(root,'build/FontTools/fonts');fs.mkdirSync(directory,{recursive:true});
fs.writeFileSync(path.join(directory,'MeasurementUIIcons.ttf'),Buffer.from(result.toArrayBuffer()));
process.stdout.write('完整图标子集及Wi-Fi断开字形已生成\n');
