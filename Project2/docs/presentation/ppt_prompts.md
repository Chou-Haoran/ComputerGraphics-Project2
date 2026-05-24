# PPT 生成提示词 — Monte Carlo Path Tracer 答辩

> **约束**：不超过 6 页幻灯片，总时长 < 4 分钟。聚焦：动机目标 → 管线概览 → 材质贴图 → 灯光环境 → 结果 → 限制改进。
> 使用方法：将每页提示词复制到 AI PPT 工具（Gamma、Beautiful.ai、WPS AI 等）逐页生成。
> 视觉风格：深色背景 + 科技蓝/青色点缀（#00D4FF / #7B68EE / #FFFFFF）。

---

## Slide 1 — 封面 + 动机

**提示词：**
```
生成一张学术答辩风格的封面幻灯片，分上下两部分。

上半部分（占60%）— 封面：
标题：Monte Carlo Path Tracer — 从渲染方程到物理真实感绘制
副标题：COMP4610/8610 Research Project 2
底部小字：姓名 / 学号 / 2026年5月
深蓝渐变背景，标题白色大号粗体，右下方放一张打字机渲染效果图水印。

下半部分（占40%）— 一句话动机，用两个对比色块：
左侧橙色块：光栅化局限 — 无全局光照 · 假反射 · 烘焙光照贴图
右侧青色块：Path Tracing — 直接求解渲染方程 · 统一处理所有光路 · 物理正确

中间箭头：光栅化 → Path Tracing，标注"从近似到物理真实"。
```

---

## Slide 2 — 渲染管线概览

**提示词：**
```
生成一张技术概览幻灯片。
标题：Rendering Pipeline Overview

左侧（60%宽度）— 管线流程图，节点用箭头连接自上而下：

节点1 "Per-Pixel Sampling"（青色）→
节点2 "Ray Generation — Camera → Image Plane"（蓝）→
节点3 "BVH Intersection — O(log n) Ray-Scene Test"（紫）→
节点4 "Direct Lighting — MIS: Light Sampling + BSDF Sampling"（橙）→
节点5 "Indirect Bounces — BSDF Importance Sampling + Russian Roulette"（红）→
节点6 "Accumulate → Tone-map → Output"（绿）

右侧（40%宽度）— 三个关键技术指标卡片：
卡片1: 6 种 BSDF 材质 (Diffuse / GGX Microfacet / Metal / Mirror / Glass / Emit)
卡片2: BVH 加速 (40万三角形场景下 ~28ms/ray)
卡片3: 多重重要性采样 (Power Heuristic β=2, 大幅降低方差)

底部一行标注：C++17 · OpenMP 多线程 · ~4000 行代码
```

---

## Slide 3 — 材质系统与 GGX 微表面

**提示词：**
```
生成一张幻灯片展示材质系统。
标题：Material System — BSDF Architecture & GGX Microfacet

上半部分 — 材质基类接口（代码风格卡片居中）：
  sample() → 重要性采样出射方向  |  eval() → 计算 BSDF 值  |  pdf() → 采样概率密度

下半部分左 — 6 种材质网格（2行×3列）：
  1. DIFFUSE — Lambertian, cos-weighted hemisphere
  2. MICROFACET — Cook-Torrance, GGX NDF
  3. METAL — GGX specular only, color=F₀
  4. MIRROR — Perfect reflection (Delta)
  5. GLASS — Fresnel refraction+reflection
  6. EMIT — Self-emissive surface

下半部分右 — GGX 核心公式卡片（LaTeX）：
  f_r = (1-m)·albedo/π + m·[D·G·F / (4|n·ω_i||n·ω_o|)]
  D: GGX法线分布 (α=roughness²)
  G: Smith-Schlick 几何遮蔽
  F: Schlick 菲涅尔近似

底部注：材质工厂自注册 — 新增材质类型无需修改中央调度代码。
```

---

## Slide 4 — 光源与环境光照

**提示词：**
```
生成一张幻灯片展示光照系统。
标题：Lighting & Environment Illumination

上半部分 — 分析光源（7种类型，用一排小图标/卡片展示）：
  AreaRect | AreaDisk | AreaSphere | Point | Spot | Directional | MeshLight
  下方标注：按亮度 CDF 重要性采样 · 半透明阴影(有色透射率累积) · delta光源自动处理

下半部分分两栏：

左栏 — HDR 环境贴图：
  - stb_image 加载 .hdr 等距柱状投影
  - 2D 亮度加权 CDF 重要性采样 (O(log n))
  - 支持旋转 + 强度缩放
  - MIS 结合 BSDF 采样降低方差

右栏 — 放一张场景对比截图：
  上图：无环境贴图 — 暗部全黑
  下图：有环境贴图 + 重要性采样 MIS — 自然柔光照亮暗部

底部标注：环境贴图来源 — 程序化生成的 studio 光照 HDR
```

---

## Slide 5 — 渲染结果

**提示词：**
```
生成一张结果展示幻灯片。
标题：Results — Rendered Images

上半部分：桌面打字机场景 (Desktop Typewriter Scene)
  参数标注：1200×800 | 64 spp | 8 bounces | Adaptive + Denoise
  横排 3 张效果图：
    左图 "Front View" — 正面视角，展示 GGX 金属高光与柔和阴影
    中图 "Side View" — 侧面打光，展示材质在不同光照下表现
    右图 "Light Types Demo" — 点光/聚光/面光/平行光同场景对比

下半部分：Blender 复古房间 (Retro Room Scene)
  参数标注：1200×600 | 64 spp | 8 bounces | envmap + window area light | Denoise
  横排 3 张效果图：
    左图 "Essential" — 核心桌椅，展示全局光照
    中图 "Grouped" — 完整房间，环境光多次弹射
    右图 "Denoised" — 降噪最终效果，高光细节与阴影层次

底部附注：场景由 Blender 导出，经 Python 脚本分割分组，.materialmap 映射材质。
```

---

## Slide 6 — 限制与改进方向

**提示词：**
```
生成一张总结展望幻灯片。
标题：Limitations & Future Work

上半部分 — 当前限制（左侧红色/橙色卡片，3项）：
  1. 无景深 — 光圈/对焦字段已预留，但透镜采样未实现
  2. BVH 用 NAIVE 中位数分割 — SAH 可提升 20-40% 遍历效率
  3. 无体积散射 — 不支持雾、烟雾等参与介质

上半部分右 — 已实现亮点总结（右侧青色卡片）：
  "从零构建完整 C++ Monte Carlo Path Tracer
   6种BSDF · 7种光源 · HDR环境光 · MIS · BVH
   自适应采样 · 联合双边滤波降噪 · 半透明阴影
   可渲染 40万三角形复杂场景的物理真实感图像"

下半部分 — 未来方向 roadmap 横排 4 个箭头节点：
  薄透镜景深 → SAH BVH → 体积散射 → GPU加速(OptiX/CUDA)

底部居中：Thank You — Q&A
右下角：参考文献缩略（Kajiya 1986, Veach 1997, Cook-Torrance 1982, Walter 2007）
```

---

## 附：备用幻灯片（不计入 6 页限制）

### 备用 — 消融实验对比

**提示词：**
```
生成一张对比分析幻灯片，用于 Q&A 环节展示。
标题：Ablation Study — 关键技术的消融对比

用 2×2 网格展示 4 组前后对比图：

左上：MIS 消融 — 无MIS（多噪声） vs 有MIS（低噪声）
右上：降噪消融 — 原始渲染（噪点） vs 联合双边滤波（平滑且边缘保持）
左下：环境光消融 — 无envmap（暗部全黑） vs 有envmap+重要性采样（自然柔光）
右下：自适应采样 — 均匀采样热力图 vs 自适应采样热力图（平坦区域提前停止）

每组对比图用 A→B 箭头标注，下附一行简短说明。
```
