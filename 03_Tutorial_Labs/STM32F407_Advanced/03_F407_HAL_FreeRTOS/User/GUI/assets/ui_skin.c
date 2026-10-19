#include "ui_skin.h"
#include "src/image/lv_image_decoder_private.h"
#include <string.h>

/* 历史图片索引/调色板与当前程序渐变描述符放Flash，只预留480像素RGB888行缓存。
 * 本工程只有一个软件绘图单元和一个GUI任务，绘制/解码串行执行，因此可共用此缓存。 */
static uint8_t line_pixels[480 * 3] __attribute__((aligned(4)));
static lv_draw_buf_t line_buffer;
static lv_image_decoder_t *skin_decoder;
/* USER2表示程序生成的背景；描述符常驻Flash，避免绘图任务保存局部变量指针。
 * 两种模式分别按最终宽度生成，保留8位颜色，最终抖色统一交给屏幕flush。 */
static const uint8_t plot_marker = 0;
static const lv_image_dsc_t plot_normal = {
    .header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB888,
             .flags=LV_IMAGE_FLAGS_USER2,.w=292,.h=232,.stride=876},
    .data_size=1,.data=&plot_marker
};
static const lv_image_dsc_t plot_quality = {
    .header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB888,
             .flags=LV_IMAGE_FLAGS_USER2,.w=278,.h=232,.stride=836},
    .data_size=1,.data=&plot_marker
};
static void plot_pixel(uint8_t *pixel,int x,int y,int width)
{
    /* 顶部高光以平方曲线平滑减弱，底部轻微提亮；整数运算减少F407绘图开销。 */
    unsigned t=(unsigned)(231-y),top=t*t*256U/(231U*231U);
    unsigned bottom=(unsigned)y*(unsigned)y*256U/(231U*231U);
    unsigned edge=(unsigned)LV_ABS(2*x-width+1)*256U/(unsigned)width;
    /* 实机反馈中间色明显比电脑深：整体抬高中间亮度，缩小渐变跨度。
     * 纯白已是RGB565满值，不能再加亮；这里调整的是雾蓝背景的中间色阶。 */
    unsigned r=205U+(top*38U+bottom*10U+edge*2U)/256U;
    unsigned g=222U+(top*23U+bottom*7U+edge*2U)/256U;
    unsigned b=236U+(top*15U+bottom*4U+edge*1U)/256U;
    /* RGB888按B/G/R存储；不要先转565再参与网格/阴影混合，避免二次量化。 */
    pixel[0]=(uint8_t)b;pixel[1]=(uint8_t)g;pixel[2]=(uint8_t)r;
}
static bool supported(const lv_image_decoder_dsc_t *d)
{
    if(d->src_type!=LV_IMAGE_SRC_VARIABLE)return false;
    const lv_image_dsc_t *s=d->src;
    if(s==&plot_normal||s==&plot_quality)return true;
    return s&&(s->header.flags&LV_IMAGE_FLAGS_USER1)&&s->header.w<=480&&s->header.h<=320&&
        s->data&&s->data_size>=512U+s->header.w*s->header.h;
}
static lv_result_t info(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *d,lv_image_header_t *h)
{
    (void)decoder;if(!supported(d))return LV_RESULT_INVALID;
    const lv_image_dsc_t *s=d->src;*h=s->header;
    h->cf=LV_COLOR_FORMAT_RGB888;h->stride=lv_draw_buf_width_to_stride(h->w,h->cf);return LV_RESULT_OK;
}
static lv_result_t open_image(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *d)
{(void)decoder;if(!supported(d))return LV_RESULT_INVALID;d->decoded=NULL;return LV_RESULT_OK;}
static lv_result_t area(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *d,const lv_area_t *full,lv_area_t *part)
{
    (void)decoder;const lv_image_dsc_t *s=d->src;
    int y=part->y1==LV_COORD_MIN?full->y1:part->y1+1;
    if(y>full->y2||full->x1<0||full->x2>=(int)s->header.w||y>=(int)s->header.h)return LV_RESULT_INVALID;
    *part=*full;part->y1=y;part->y2=y;int w=full->x2-full->x1+1;
    if(s==&plot_normal||s==&plot_quality) {
        for(int x=0;x<w;++x)plot_pixel(line_pixels+x*3,full->x1+x,y,s->header.w);
    } else {
        const uint8_t *indexes=s->data+512+y*s->header.w+full->x1;
        /* 历史I8资源仍能显示；将调色板的565分量完整展开，兼容保留的旧皮肤。 */
        for(int x=0;x<w;++x){unsigned offset=indexes[x]*2U;
            uint16_t c=(uint16_t)(s->data[offset]|s->data[offset+1]<<8);
            line_pixels[x*3]=(uint8_t)((c&31U)*255U/31U);
            line_pixels[x*3+1]=(uint8_t)(((c>>5)&63U)*255U/63U);
            line_pixels[x*3+2]=(uint8_t)((c>>11)*255U/31U);}
    }
    memset(&line_buffer,0,sizeof(line_buffer));line_buffer.header.magic=LV_IMAGE_HEADER_MAGIC;
    line_buffer.header.cf=LV_COLOR_FORMAT_RGB888;line_buffer.header.w=(uint16_t)w;line_buffer.header.h=1;
    line_buffer.header.stride=lv_draw_buf_width_to_stride(w,LV_COLOR_FORMAT_RGB888);
    line_buffer.data=line_pixels;line_buffer.data_size=line_buffer.header.stride;
    d->decoded=&line_buffer;return LV_RESULT_OK;
}
static void close_image(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *d)
{(void)decoder;d->decoded=NULL; /* 缓存为静态共享对象，不进行动态释放。 */}
void ui_skin_init(void)
{
    if(skin_decoder)return;
    skin_decoder=lv_image_decoder_create();LV_ASSERT_MALLOC(skin_decoder);
    lv_image_decoder_set_info_cb(skin_decoder,info);lv_image_decoder_set_open_cb(skin_decoder,open_image);
    lv_image_decoder_set_get_area_cb(skin_decoder,area);lv_image_decoder_set_close_cb(skin_decoder,close_image);
}
void ui_skin_draw(lv_layer_t *layer,const lv_image_dsc_t *source,int x,int y)
{
    lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=source;
    lv_area_t a={x,y,x+(int)source->header.w-1,y+(int)source->header.h-1};lv_draw_image(layer,&d,&a);
}
void ui_skin_draw_plot(lv_layer_t *layer,int x,int y,bool quality_mode)
{
    ui_skin_draw(layer,quality_mode?&plot_quality:&plot_normal,x,y);
}
