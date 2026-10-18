#include "ui_skin.h"
#include "src/image/lv_image_decoder_private.h"
#include <string.h>

/* 历史图片索引/调色板与当前程序渐变描述符放Flash，只预留480像素RGB565行缓存。
 * 本工程只有一个软件绘图单元和一个GUI任务，绘制/解码串行执行，因此可共用此缓存。 */
static uint16_t line_pixels[480] __attribute__((aligned(4)));
static lv_draw_buf_t line_buffer;
static lv_image_decoder_t *skin_decoder;
/* USER2表示程序生成的背景；描述符常驻Flash，避免绘图任务保存局部变量指针。
 * 两种模式分别按最终宽度生成，抖动位置取图内坐标，局部刷新时不会产生接缝。 */
static const uint8_t plot_marker = 0;
static const lv_image_dsc_t plot_normal = {
    .header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,
             .flags=LV_IMAGE_FLAGS_USER2,.w=292,.h=232,.stride=584},
    .data_size=1,.data=&plot_marker
};
static const lv_image_dsc_t plot_quality = {
    .header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,
             .flags=LV_IMAGE_FLAGS_USER2,.w=278,.h=232,.stride=556},
    .data_size=1,.data=&plot_marker
};
/* 8×8阈值表让相邻像素分担一档RGB565色差。图案固定，不随刷新改变，避免闪烁。
 * 这里只打散不足一档的误差；不会把大色块或随机噪声混进背景。 */
static const uint8_t bayer[8][8] = {
    {0,48,12,60,3,51,15,63},{32,16,44,28,35,19,47,31},
    {8,56,4,52,11,59,7,55},{40,24,36,20,43,27,39,23},
    {2,50,14,62,1,49,13,61},{34,18,46,30,33,17,45,29},
    {10,58,6,54,9,57,5,53},{42,26,38,22,41,25,37,21}
};
static unsigned channel_level(unsigned value,unsigned levels,unsigned threshold)
{
    /* value是0～255亮度，levels是31或63；余数决定本像素是否取较亮的一档。 */
    unsigned scaled=value*levels;
    return scaled/255U+((scaled%255U)*64U>threshold*255U+127U);
}
static uint16_t plot_pixel(int x,int y,int width)
{
    /* 顶部高光以平方曲线平滑减弱，底部轻微提亮；整数运算减少F407绘图开销。 */
    unsigned t=(unsigned)(231-y),top=t*t*256U/(231U*231U);
    unsigned bottom=(unsigned)y*(unsigned)y*256U/(231U*231U);
    unsigned edge=(unsigned)LV_ABS(2*x-width+1)*256U/(unsigned)width;
    /* 实机反馈中间色明显比电脑深：整体抬高中间亮度，缩小渐变跨度。
     * 纯白已是RGB565满值，不能再加亮；这里调整的是雾蓝背景的中间色阶。 */
    unsigned r=192U+(top*48U+bottom*12U+edge*3U)/256U;
    unsigned g=213U+(top*30U+bottom*9U+edge*2U)/256U;
    unsigned b=233U+(top*18U+bottom*5U+edge*1U)/256U;
    unsigned threshold=bayer[y&7][x&7];
    return (uint16_t)((channel_level(r,31,threshold)<<11)|
                      (channel_level(g,63,threshold)<<5)|channel_level(b,31,threshold));
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
    h->cf=LV_COLOR_FORMAT_RGB565;h->stride=h->w*2;return LV_RESULT_OK;
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
        for(int x=0;x<w;++x)line_pixels[x]=plot_pixel(full->x1+x,y,s->header.w);
    } else {
        const uint8_t *indexes=s->data+512+y*s->header.w+full->x1;
        for(int x=0;x<w;++x){unsigned offset=indexes[x]*2U;line_pixels[x]=(uint16_t)(s->data[offset]|s->data[offset+1]<<8);}
    }
    memset(&line_buffer,0,sizeof(line_buffer));line_buffer.header.magic=LV_IMAGE_HEADER_MAGIC;
    line_buffer.header.cf=LV_COLOR_FORMAT_RGB565;line_buffer.header.w=(uint16_t)w;line_buffer.header.h=1;
    line_buffer.header.stride=(uint16_t)(w*2);line_buffer.data=(uint8_t *)line_pixels;line_buffer.data_size=(uint32_t)w*2;
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
