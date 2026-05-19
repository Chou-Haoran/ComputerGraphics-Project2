# Project2 渲染器

这是一个面向 COMP4610/8610 Project 2 的蒙特卡洛路径追踪渲染器。当前代码库支持用纯文本 `.scene` 文件描述 OBJ 场景，支持多种材质、多种灯光类型、HDR 环境光、贴图驱动的材质外观，以及基于 OpenMP 的多线程渲染。

## 当前功能

- 基于路径追踪的全局光照。
- 面向三角网格和基础几何体的 BVH 加速。
- 基于工厂和自注册机制的材质系统。
- 已支持材质类型：`DIFFUSE`、`MICROFACET`、`METAL`、`MIRROR`、`GLASS`、`EMIT`。
- 已支持贴图输入：漫反射、法线、凹凸、粗糙度、光泽度。
- 已支持场景灯光类型：`AREA_RECT`、`AREA_DISK`、`AREA_SPHERE`、`POINT`、`SPOT`、`DIRECTIONAL`。
- 自发光网格已接入统一的直接光采样路径。
- HDR 环境光支持直接采样与 MIS。
- 相机支持 look-at、视场角、光圈、对焦距离。
- 渲染器支持 OpenMP 行级并行和线程局部随机数。
- 输出 `PPM` 和 `PNG`。

## 项目结构

```text
src/
  camera/     相机模型与主光线生成
  core/       数学、包围盒、射线、随机数、公共常量
  geometry/   OBJ 网格、三角形、球体、BVH 接口
  light/      灯光系统、环境光、统一光采样
  material/   材质实现、贴图、材质工厂
  renderer/   渲染主循环与图像输出
  scene/      场景积分器与 .scene 解析器
  main.cpp    CLI 入口

assets/
  envmaps/    HDR 环境图
  models/     场景目录，包含 OBJ 与 .scene 文件
  textures/   共享贴图资源

configs/      场景语法与编辑说明
scripts/      辅助脚本
output/       渲染输出
```

`src` 下每个模块都带有独立 README。

## 编译

```bash
cmake -S . -B build
cmake --build build -j8
```

## 运行

```bash
./build/Project2Renderer <scene_dir> <view_name> [WxH] [spp] [flags]
```

示例：

```bash
./build/Project2Renderer assets/models/desktop_scene typewriter_table_showcase 960x720 64
./build/Project2Renderer assets/models/desktop_scene light_types_demo 800x600 32
./build/Project2Renderer assets/models/desktop_scene area_light_shapes_demo 800x600 32
```

## 场景文件编辑

完整的 `.scene` 语法说明和中文示例见 `configs/README.md`。

## 文档索引

- `configs/README.md`：场景语法、编辑流程、模板示例
- `assets/models/README.md`：模型目录约定
- `assets/models/desktop_scene/README.md`：桌面场景资源说明
- `assets/models/desktop_scene/typewriter/README.md`：打字机资源与材质映射
- `assets/envmaps/README.md`：HDR 环境光说明
- `assets/textures/README.md`：贴图使用说明
- `scripts/README.md`：辅助脚本说明
- `docs/`：动机图、报告、答辩文档说明
