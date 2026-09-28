# 素材记录

角色名称：栗栗。原创建模风格的小刺猬桌宠。
制作方式：内置图像生成工具；未调用外部图像 API。
素材：`assets/hedgehog-atlas.png`，1536 × 1024，RGBA，3 列 × 2 行。
程序直接加载透明 PNG 图集，运行时按六个网格读取角色边界。
`assets/lili.ico` 是第一格角色转换得到的 Windows 图标。

六个姿态依次为：站立、眨眼、散步 A、散步 B、睡觉、开心。
呼吸、上下摆动、镜像转向、爱心和文字气泡由程序实时绘制。

生成提示词：

Use case: stylized-concept. Asset type: sprite atlas for a Windows desktop companion. Create one clean 3 columns by 2 rows sprite sheet with EXACTLY SIX isolated full-body poses of the SAME adorable round baby hedgehog, a soft 3D plush toy with warm chestnut quills, cream face and tummy, small glossy black eyes, pink cheeks, tiny paws, calm charming expression. Transparent background with real alpha, no floor, no panel backgrounds, no cast shadows, no text, no letters, no grid lines. Canvas landscape 1536x1024, six exactly equal 512x512 square cells; each character fully contained centered within its cell, ample transparent padding on all sides, feet aligned at the same height within every cell. Character occupies about 75% of cell height. Read left-to-right top-to-bottom: (1) front three-quarter facing slightly right, standing alert idle; (2) same angle and body, blink with closed eyes; (3) same character walking towards right, one foot forward; (4) same character walking towards right, opposite foot forward; (5) same character sitting asleep curled slightly, eyes closed, no symbols; (6) same character delighted, both tiny paws raised smiling. Consistent scale, design, colors, camera, lighting, silhouette proportions across cells. No props, no watermark. Polished game-ready sprite asset with crisp transparent edges.

# Windows 技术参考

- https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/nf-winuser-updatelayeredwindow
- https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-blendfunction

透明窗口使用 32 位预乘 BGRA，源透明度随图集保留。

# 2.0 新增图集

assets/hedgehog-extra.png：1536 × 1024 透明 RGBA，内置图像生成工具，以原图集为角色参考。
新增伸懒腰、打哈欠、挠脸、打滚、嗅闻、生病六个姿态。

本次生成提示词：

Use case: stylized-concept. The reference image is the character identity reference for a desktop pet named Lili. Generate a NEW sprite atlas with six additional poses of EXACTLY the same adorable chestnut-and-cream plush baby hedgehog. Keep its round body, quills, big dark eyes, tiny pink paws, warm soft lighting, rendering and proportions. Landscape 1536 x 1024, strict 3 columns x 2 rows of equal 512x512 cells. One complete isolated character in each cell, centered, feet or ground baseline near 460 in each local cell, every extremity contained with at least 24px transparent margin. TRUE TRANSPARENT BACKGROUND, no scenery, no floor, no opaque background, no cast shadows, no letters, no accessories. Six different poses read left-to-right top-to-bottom: 1 stretching both arms upward and leaning back, closed contented eyes; 2 yawning sleepily with tiny open mouth and one paw near mouth; 3 scratching one cheek with a paw, slightly tilted curious head; 4 lying on its back playfully rolling, paws up, delighted face, whole body visible; 5 leaning forward sniffing curiously at ground, tiny nose forward, eyes looking down; 6 looking mildly unwell and tired, lowered ears and slightly droopy eyes, seated with a paw on tummy, still cute, no thermometer or props. All six use the same consistent character scale. Transparent edges for game sprites.

2.0 API 参考：
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey
- https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifileoperation-setoperationflags
- https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifileoperationprogresssink-predeleteitem
- https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifileoperationprogresssink-postdeleteitem

界面预览采用源代码的布局参数配合 SVG/CairoSVG 导出，图标有所简化，不是 Windows 实机截图。

# 2.1 连续动作图集

新增 motion-walk.png、motion-relax.png、motion-play.png，各 1254 × 1254 RGBA，4 列 × 4 行；共 48 帧。
使用内置图像生成工具，以原 hedgehog-atlas.png 为角色参考，保留颜色、造型和透明通道。
分别生成：八帧走路循环、四帧眨眼、四帧开心；八帧伸懒腰、八帧哈欠；八帧挠脸、八帧打滚。
三次提示均要求同一相机、尺寸和脚底基线、完整角色、真正透明背景，无地板阴影、文字、网格。
完整生成说明保存在 animation-art-prompts.txt。

图像工具输出边距并非严格等宽，因此没有直接用宽度除四取格。build_sprite_metadata.py 只分析透明通道的独立角色区域，输出 60 帧的坐标和脚底锚点，原始图集不做修改。
sprite_metadata.h / assets/sprite-metadata.json 固定这些导入参数；运行时仅使用 Windows 自带 GDI+，不依赖 Python 或 SciPy。
animation.h 提供 Windows 程序、测试和离线预览共用的时间轴。所有姿态使用统一画布，预乘像素线性混合，变换持续插值。
preview_animation.py 导出的 GIF/MP4 是共用时间轴的离线预览，非 Windows 实机录屏；素材原图未在预览制作中修改。

2.1 插值补充：初版离线预览暴露出直接叠帧的五官重影，现使用 build_motion_vectors.py 计算 94 对图帧的位移元数据（819320 字节），运行时 motion_flow.h 使用这些向量移动图像采样位置，再做透明度混合。开发分析使用 OpenCV DIS；Windows 程序只嵌入向量，不依赖 OpenCV。源图集保持逐字节不变。预览直接调用相同 C++ 插值内核。
