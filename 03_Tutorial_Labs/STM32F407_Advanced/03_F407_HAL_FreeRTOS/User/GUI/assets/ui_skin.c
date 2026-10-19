#include "ui_skin.h"
#include "src/image/lv_image_decoder_private.h"
#include <string.h>

/* 只有CPU读取的解码行，不作为DMA源；最终由LVGL混合到普通SRAM双缓冲。
 * 所有静态渐变已在Flash预计算并按8像素块压缩，刷新时仅查表/复制。 */
static uint16_t line_pixels[480] __attribute__((aligned(4)));
static lv_draw_buf_t line_buffer;
static lv_image_decoder_t *skin_decoder;
/* 单GUI任务、单软件绘图单元、唯一几何视图：每条带的任务处理完才进入下一条带。
 * 此描述符在同一条带内不再修改，行解码只读取它；不能用于多个并发材质实例。 */
static uint8_t material_marker;
static lv_image_dsc_t material={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,
    .flags=LV_IMAGE_FLAGS_USER2},.data_size=1,.data=&material_marker};
static int material_x,material_y;
static const uint8_t bayer[8][8]={
    {0,48,12,60,3,51,15,63},{32,16,44,28,35,19,47,31},
    {8,56,4,52,11,59,7,55},{40,24,36,20,43,27,39,23},
    {2,50,14,62,1,49,13,61},{34,18,46,30,33,17,45,29},
    {10,58,6,54,9,57,5,53},{42,26,38,22,41,25,37,21}};
static unsigned quant(unsigned value,unsigned levels,unsigned threshold)
{
    unsigned scaled=value*levels;
    return scaled/255U+((scaled%255U)*64U>threshold*255U+127U);
}
typedef struct { uint16_t end, tile; } UiPerfRun;
typedef struct {
    lv_image_dsc_t image;
    const uint16_t *rows;
    const UiPerfRun *runs;
    uint32_t top,bottom;
} UiPerfImage;
#include "ui_perf_tables.inc"
static const UiPerfImage *perf_source(const void *source)
{
    for(unsigned i=0;i<sizeof(ui_perf_images)/sizeof(ui_perf_images[0]);++i)
        if(source==&ui_perf_images[i].image) return &ui_perf_images[i];
    return NULL;
}
static bool supported(const lv_image_decoder_dsc_t *d)
{
    if(d->src_type!=LV_IMAGE_SRC_VARIABLE)return false;
    const lv_image_dsc_t *s=d->src;
    if(s==&material||perf_source(s))return true;
    return s&&(s->header.flags&LV_IMAGE_FLAGS_USER1)&&s->header.w<=480&&s->header.h<=320&&
        s->data&&s->data_size>=512U+s->header.w*s->header.h;
}
static lv_result_t info(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *d,lv_image_header_t *h)
{
    (void)decoder;if(!supported(d))return LV_RESULT_INVALID;
    const lv_image_dsc_t *s=d->src;*h=s->header;
    h->cf=LV_COLOR_FORMAT_RGB565;h->stride=lv_draw_buf_width_to_stride(h->w,h->cf);return LV_RESULT_OK;
}
static lv_result_t open_image(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *d)
{(void)decoder;if(!supported(d))return LV_RESULT_INVALID;d->decoded=NULL;return LV_RESULT_OK;}
static lv_result_t area(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *d,const lv_area_t *full,lv_area_t *part)
{
    (void)decoder;const lv_image_dsc_t *s=d->src;
    int y=part->y1==LV_COORD_MIN?full->y1:part->y1+1;
    if(y>full->y2||full->x1<0||full->x2>=(int)s->header.w||y>=(int)s->header.h)return LV_RESULT_INVALID;
    *part=*full;part->y1=y;part->y2=y;int w=full->x2-full->x1+1;
    const UiPerfImage *precomputed=perf_source(s);
    if(s==&material) {
        /* 任意高度的矩形仍保留平滑渐变。只按本行计算一次8位RGB和8个565相位，
         * 随后平铺；坐标取完整图像位置，局部x/y不会重启抖色。 */
        unsigned last=255U*s->header.h/256U;
        unsigned mix=(unsigned)y>=last?255U:(unsigned)y*255U/last;
        unsigned r=(239U*(255U-mix)+174U*mix)*0x8081U>>23;
        unsigned g=(248U*(255U-mix)+199U*mix)*0x8081U>>23;
        unsigned b=(255U*(255U-mix)+218U*mix)*0x8081U>>23;
        uint16_t tile[8];
        for(unsigned x=0;x<8;++x) {
            unsigned t=bayer[(material_y+y)&7][(material_x+(int)x)&7];
            tile[x]=(uint16_t)(quant(r,31,t)<<11|quant(g,63,t)<<5|quant(b,31,t));
        }
        for(int x=0;x<w;++x)line_pixels[x]=tile[(full->x1+x)&7];
    } else if(precomputed) {
        /* rows直接定位本行；runs的end是相同8像素块的结束列，任意局部x起点均可寻址。 */
        unsigned run=precomputed->rows[y];
        for(int x=0;x<w;++x) {
            unsigned column=(unsigned)(full->x1+x);
            while(column/8U>=precomputed->runs[run].end) ++run;
            line_pixels[x]=ui_perf_tiles[precomputed->runs[run].tile][column&7U];
        }
    } else {
        /* 保留历史I8皮肤契约，直接展开为565，无需888中转或动态分配。 */
        const uint8_t *indexes=s->data+512+y*s->header.w+full->x1;
        for(int x=0;x<w;++x){unsigned offset=indexes[x]*2U;
            line_pixels[x]=(uint16_t)(s->data[offset]|s->data[offset+1]<<8);}
    }
    memset(&line_buffer,0,sizeof(line_buffer));line_buffer.header.magic=LV_IMAGE_HEADER_MAGIC;
    line_buffer.header.cf=LV_COLOR_FORMAT_RGB565;line_buffer.header.w=(uint16_t)w;line_buffer.header.h=1;
    line_buffer.header.stride=lv_draw_buf_width_to_stride(w,LV_COLOR_FORMAT_RGB565);
    line_buffer.data=(uint8_t *)line_pixels;line_buffer.data_size=line_buffer.header.stride;
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
    ui_skin_draw(layer,&ui_perf_images[quality_mode?1:0].image,x,y);
}

bool ui_skin_draw_gradient(lv_layer_t *layer,int x,int y,int w,int h,
                            uint32_t top,uint32_t bottom,int radius)
{
    const lv_image_dsc_t *source=ui_skin_gradient_source(w,h,top,bottom);
    if(!source)return false;
    lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=source;d.clip_radius=radius;
    lv_area_t a={x,y,x+w-1,y+h-1};lv_draw_image(layer,&d,&a);return true;
}
const lv_image_dsc_t *ui_skin_gradient_source(int w,int h,uint32_t top,uint32_t bottom)
{
    /* 只替换预计算的固定卡片渐变；任意缩放工件仍走原LVGL渐变，不建立无界缓存。 */
    for(unsigned i=2;i<sizeof(ui_perf_images)/sizeof(ui_perf_images[0]);++i) {
        const UiPerfImage *s=&ui_perf_images[i];
        if(s->image.header.w!=w || s->image.header.h!=h || s->top!=top || s->bottom!=bottom) continue;
        return &s->image;
    }
    return NULL;
}
void ui_skin_draw_material(lv_layer_t *layer,int x,int y,int w,int h)
{
    material.header.w=(uint16_t)w;material.header.h=(uint16_t)h;material.header.stride=(uint16_t)(w*2);
    material_x=x;material_y=y;
    lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=&material;d.clip_radius=4;
    lv_area_t a={x,y,x+w-1,y+h-1};lv_draw_image(layer,&d,&a);
}
