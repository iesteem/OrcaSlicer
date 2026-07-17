#ifndef _prusaslicer_technologies_h_
#define _prusaslicer_technologies_h_

//=============
// 调试技术
//=============
// 在3D场景中显示相机目标
#define ENABLE_SHOW_CAMERA_TARGET 0
// 在选择更改时将调试消息记录到控制台
#define ENABLE_SELECTION_DEBUG_OUTPUT 0
// 当没有gizmo激活时，在当前选择边界框的中心渲染一个小球
#define ENABLE_RENDER_SELECTION_CENTER 0
// 显示与相机相关的imgui对话框
#define ENABLE_CAMERA_STATISTICS 0
// 启用从选定的gcode中提取缩略图并将其保存为png文件
#define ENABLE_THUMBNAIL_GENERATOR_DEBUG 0
// 禁用未选定实例的同步
#define DISABLE_INSTANCES_SYNCH 0
// 使用wxDataViewRender代替wxDataViewCustomRenderer
#define ENABLE_NONCUSTOM_DATA_VIEW_RENDERING 0
// 启用G-Code查看器统计信息imgui对话框
#define ENABLE_GCODE_VIEWER_STATISTICS 0
// 启用G-Code查看器在从gcode检测到的刀具路径高度和宽度与gcode生成时计算的值之间进行比较 
#define ENABLE_GCODE_VIEWER_DATA_CHECKING 0
// 启用项目脏状态管理器调试窗口
#define ENABLE_PROJECT_DIRTY_STATE_DEBUG_WINDOW 0


// 启用使用环境贴图渲染对象
#define ENABLE_ENVIRONMENT_MAP 0
// 启用对象法线平滑
#define ENABLE_SMOOTH_NORMALS 0
// 启用将预览中的选项标记渲染为固定屏幕大小的点
#define ENABLE_FIXED_SCREEN_SIZE_POINT_MARKERS 1

// 在开发模式下启用样式编辑器
#define ENABLE_IMGUI_STYLE_EDITOR	0

// 启用从磁盘重新加载命令的重构
#define ENABLE_RELOAD_FROM_DISK_REWORK 1

//====================
// 2.4.0.beta1 techs
//====================
#define ENABLE_2_4_0_BETA1 1

// 启用始终将修改器和类似对象渲染为透明
#define ENABLE_MODIFIERS_ALWAYS_TRANSPARENT (1 && ENABLE_2_4_0_BETA1)


//====================
// 2.4.0.beta2 techs
//====================
#define ENABLE_2_4_0_BETA2 1

// 启用修改后的ImGuiWrapper::slider_float()来创建一个复合控件，其中
// 可以使用额外的按钮将键盘焦点设置到滑块中
// 以允许用户输入所需的值
#define ENABLE_ENHANCED_IMGUI_SLIDER_FLOAT (1 && ENABLE_2_4_0_BETA2)
// 启用为圆形打印床适配打印体积的命令
#define ENABLE_ENHANCED_PRINT_VOLUME_FIT (1 && ENABLE_2_4_0_BETA2)
// 启用使用光线追踪进行拾取
#define ENABLE_RAYCAST_PICKING_DEBUG 0


#endif // _prusaslicer_technologies_h_
