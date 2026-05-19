# src

`src/` 是整个渲染器的核心源码目录。这里的代码按职责拆分。

从整体上看，这个项目的工作流是：

1. `main.cpp` 读取命令行参数。
2. `scene/` 解析 `.scene` 文件并构建场景对象。
3. `geometry/` 加载 OBJ、建立几何体和 BVH。
4. `material/` 为物体分配材质和贴图。
5. `light/` 构建解析灯光、自发光网格灯和环境光。
6. `camera/` 根据相机参数生成主光线。
7. `renderer/` 调用 `scene/` 的积分器完成逐像素渲染，执行自适应采样与内置降噪，并输出图像。

## 模块列表

- `camera/`：相机模型与主光线生成
- `core/`：数学类型、射线、包围盒、随机数、公共常量
- `geometry/`：物体接口、网格、三角形、球体、BVH
- `light/`：解析灯光、自发光网格、环境图采样
- `material/`：材质基类、具体 BSDF、贴图、材质工厂
- `renderer/`：多线程渲染主循环、自适应采样、内置降噪与图像输出
- `scene/`：路径积分器与 `.scene` 解析器
- `main.cpp`：CLI 与场景加载入口

这些 README 现在不只是“目录说明”，而是按下面的目标来写的：

- 解释这个模块在整条渲染管线中的位置
- 说明它和其他模块怎么配合
- 给出常见使用方式
- 说明增删改功能时应该改哪里
- 尽量把“课程项目里真正会遇到的改动路径”写清楚

## 模块协作关系

- `main.cpp` 是程序入口，负责接收 `<scene_dir> <view_name>`、分辨率、采样数和 CLI 覆盖参数。
- `scene/SceneLoader` 会把 `.scene`、`.materialmap`、OBJ/MTL 资源解析成运行时描述，并创建真正的 `Scene`。
- `geometry/` 负责“场景里有什么东西”：例如三角网格、球体、包围盒、BVH 和相交数据结构。
- `material/` 负责“这些表面看起来怎样”：例如漫反射、金属、微表面、镜面、玻璃、自发光，以及贴图取样。
- `light/` 负责“光从哪里来”：包括解析灯光、环境图采样、自发光网格接入统一灯光接口。
- `camera/` 负责“从哪里看场景”：把图像平面的采样点映射成世界空间光线。
- `scene/Scene.cpp` 负责“如何积分”：它把相交、材质、灯光、环境光、俄罗斯轮盘赌和 MIS 组合成最终的路径追踪逻辑。
- `renderer/` 负责“如何跑完整张图”：包括多线程循环、样本累积、自适应采样、内置 joint bilateral 降噪和图片输出。

## 常见阅读顺序

如果你第一次读这个项目，建议按下面的顺序看：

1. `main.cpp`
2. `scene/README.md`
3. `scene/SceneLoader.cpp`
4. `scene/Scene.cpp`
5. `material/README.md`
6. `light/README.md`
7. `geometry/README.md`
8. `renderer/README.md`

这样比较容易先建立“程序是怎么跑起来的”整体认识，再回头看具体实现细节。

如果你当前是带着具体任务来读代码，也可以按下面的目的直接跳：

- 想搭场景：先看 `scene/README.md` 和 `configs/README.md`
- 想改材质分配：先看 `material/README.md`
- 想改灯光效果：先看 `light/README.md`
- 想改收敛速度和输出图：先看 `renderer/README.md`
- 想加新几何体：先看 `geometry/README.md`

## 常见改动应该去哪

- 想加新材质：看 `material/`
- 想加新灯光类型：看 `light/` 和 `scene/SceneLoader.cpp`
- 想扩展 `.scene` 语法：看 `scene/SceneLoader.cpp`
- 想改路径追踪逻辑：看 `scene/Scene.cpp`
- 想改 OBJ 导入、变换、采样：看 `geometry/`
- 想改相机、景深：看 `camera/`
- 想改多线程渲染循环、自适应采样、降噪或输出格式：看 `renderer/`

如果你的需求是“场景和材质都要改”，推荐顺序是：

1. 先改 `.scene`
2. 再改 `.materialmap`
3. 必要时再回 `SceneLoader.cpp` 或 `material/` 改实现

这样可以最大化复用现有语法和工作流，少动底层代码。

## 关键设计约定

- 材质系统使用“抽象基类 + 工厂 + 自注册”结构，因此新增材质时不需要回到中心文件改大 `switch`。
- 灯光系统已经统一到 `Light` 接口，解析灯光、自发光 mesh 和环境光都尽量走统一采样路径。
- `.scene` 文件是主场景描述入口，`.materialmap` 是 OBJ/MTL 到内部材质系统的中间映射层。
- 运行时大量公共数学、随机数和常量定义放在 `core/` 中，避免循环依赖和重复实现。

## 建议配合阅读的文档

- 根目录 [README.md](file:///Users/kyviii/Library/CloudStorage/OneDrive-%E5%85%B1%E4%BA%AB%E7%9A%84%E5%BA%93-Onedrive/_Aus/_ANU%20Study/26S1/CG/C-Lab-4-v3-codebase2/Project2/README.md)
- 场景文件说明 [configs/README.md](file:///Users/kyviii/Library/CloudStorage/OneDrive-%E5%85%B1%E4%BA%AB%E7%9A%84%E5%BA%93-Onedrive/_Aus/_ANU%20Study/26S1/CG/C-Lab-4-v3-codebase2/Project2/configs/README.md)
- 灯光模块说明 [light/README.md](file:///Users/kyviii/Library/CloudStorage/OneDrive-%E5%85%B1%E4%BA%AB%E7%9A%84%E5%BA%93-Onedrive/_Aus/_ANU%20Study/26S1/CG/C-Lab-4-v3-codebase2/Project2/src/light/README.md)
- 材质模块说明 [material/README.md](file:///Users/kyviii/Library/CloudStorage/OneDrive-%E5%85%B1%E4%BA%AB%E7%9A%84%E5%BA%93-Onedrive/_Aus/_ANU%20Study/26S1/CG/C-Lab-4-v3-codebase2/Project2/src/material/README.md)
- 场景模块说明 [scene/README.md](file:///Users/kyviii/Library/CloudStorage/OneDrive-%E5%85%B1%E4%BA%AB%E7%9A%84%E5%BA%93-Onedrive/_Aus/_ANU%20Study/26S1/CG/C-Lab-4-v3-codebase2/Project2/src/scene/README.md)
