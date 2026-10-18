#ifndef GUI_CAMERA_CONFIG_H
#define GUI_CAMERA_CONFIG_H
#include "instrument/instrument_model.h"

/* 固定相机的一次性参数集中在这里，不向操作员提供标定页面。
 * 当前参数对应原模拟夹具：图像原点(80,40)，每毫米8个像素。
 * 真实MaixCAM2安装后，必须用实际镜头/工作距离计算并替换这组参数；
 * 相机固定只能省去重复操作，不能证明这组模拟参数适合真实硬件。
 * homography为像素到毫米的3×3变换：X=(px-80)/8，Y=(py-40)/8。
 * simulated=true明确表示目前不是实机标定结果。 */
static inline void GUI_ApplyFixedCamera(InstrumentModel *model)
{
    double factor=model->settings.image_profile?2.0:1.0;
    model->calibration=(InstrumentCalibration){
        .valid=true,.simulated=true,.version=1,
        .image_profile=model->settings.image_profile,.reference_cmm=5000,.error_cmm=0,
        .homography={0.125/factor,0,-10,0,0.125/factor,-5,0,0,1}
    };
}
#endif
