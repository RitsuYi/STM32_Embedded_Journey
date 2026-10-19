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

/* 页脚另用统一16px画布的原创轮廓图标，避免把20px粗实心图标硬塞进33px按钮。
 * 这里用矢量路径描述轮廓，再交给同一字体转换器做4bpp抗锯齿，不缩小现成位图。 */
function footerPath() {
    const p = new opentype.Path();
    // 设计坐标为16×16，字体坐标的Y轴向上；32字体单位对应最终1像素。
    p.m = (x,y) => p.moveTo(x*32,(16-y)*32);
    p.l = (x,y) => p.lineTo(x*32,(16-y)*32);
    p.q = (cx,cy,x,y) => p.quadraticCurveTo(cx*32,(16-cy)*32,x*32,(16-y)*32);
    p.polygon = points => {p.m(...points[0]);points.slice(1).forEach(v=>p.l(...v));p.close();};
    p.circle = (r,reverse=false) => {
        const points=Array.from({length:48},(_,i)=>{
            const a=(reverse?-i:i)*Math.PI/24;return [8+r*Math.cos(a),8+r*Math.sin(a)];});
        p.polygon(points);
    };
    return p;
}
const footer = [new opentype.Glyph({name:'.notdef',advanceWidth:512,path:new opentype.Path()})];
function addFooter(cp,name,p) {footer.push(new opentype.Glyph({name,unicode:cp,advanceWidth:512,path:p}));}
const shield=footerPath();
shield.m(8,.8);shield.l(14,3);shield.l(14,7.7);shield.q(14,11.4,8,15.2);
shield.q(2,11.4,2,7.7);shield.l(2,3);shield.close();
// 内轮廓按相反方向闭合，形成透明孔；勾号保持独立，不能把盾牌填成黑块。
shield.m(8,2.2);shield.l(3.3,3.9);shield.l(3.3,7.7);shield.q(3.3,10.5,8,13.7);
shield.q(12.7,10.5,12.7,7.7);shield.l(12.7,3.9);shield.close();
shield.polygon([[4.5,7.7],[5.4,6.8],[7.2,8.6],[10.8,5.2],[11.7,6.1],[7.2,10.4]]);
addFooter(0xE101,'inspection-outline',shield);
const ruler=footerPath();
ruler.polygon([[1.5,10],[10,1.5],[14.5,6],[6,14.5]]);
ruler.polygon([[3.3,10],[6,12.7],[12.7,6],[10,3.3]]);
// 刻度与边框相连，长短交替；采用闭合轮廓，避免逐条描线的端点缺失。
for(let i=0;i<5;i++) {
    const x=8.9-i*1.4,y=2.6+i*1.4,len=i%2?.9:1.7,t=.35;
    ruler.polygon([[x-t,y+t],[x+t,y-t],[x+t+len,y-t+len],[x-t+len,y+t+len]]);
}
addFooter(0xE102,'ruler-outline',ruler);
const gear=footerPath();
function cog(r1,r2,reverse=false) {
    const points=[];
    for(let tooth=0;tooth<8;tooth++)for(let i=0;i<4;i++) {
        const angle=(tooth*45+[-17,-10,10,17][i])*Math.PI/180,r=(i===1||i===2)?r2:r1;
        points.push([8+r*Math.cos(angle),8+r*Math.sin(angle)]);
    }
    gear.polygon(reverse?points.reverse():points);
}
cog(5.6,7.2);cog(4.5,5.9,true);gear.circle(2.5);gear.circle(1.4,true);
addFooter(0xE103,'settings-outline',gear);
const play=footerPath();play.polygon([[5,3],[13,8],[5,13]]);addFooter(0xE104,'play-small',play);
const stop=footerPath();stop.polygon([[4,4],[12,4],[12,12],[4,12]]);addFooter(0xE105,'stop-small',stop);
const footerFont=new opentype.Font({familyName:'Measurement Footer Icons',styleName:'Regular',
    unitsPerEm:512,ascender:512,descender:0,glyphs:footer});
fs.writeFileSync(path.join(directory,'MeasurementFooterIcons.ttf'),Buffer.from(footerFont.toArrayBuffer()));
process.stdout.write('16px页脚轮廓图标已生成\n');
