# geometry 模块

这个模块负责“场景里到底有什么物体，以及光线如何和它们相交”。

如果没有这个模块，渲染器就只有光线和材质概念，却不知道：

- 哪些地方有表面
- 法线和纹理坐标是什么
- 面积灯该从哪里采样
- 怎样高效地做相交测试

## 这个模块包含什么

- `Object.hpp`：统一物体接口，所有可渲染对象都要实现它
- `Intersection.hpp`：相交结果结构，保存命中点、法线、材质、纹理坐标等信息
- `Triangle.hpp`：单个三角形，以及 OBJ 网格版 `MeshTriangle`
- `Sphere.hpp`：球体原语
- `BVH.hpp` / `BVH.cpp`：包围体层次结构，用于加速相交
- `OBJ_Loader.hpp`：Wavefront OBJ 加载器

## 它在整个程序里的位置

典型流程是：

1. `scene/SceneLoader.cpp` 解析 `.scene` 中的 `mesh "..." { ... }`
2. 读入 OBJ，并为不同子网格分配材质
3. `geometry/` 把 OBJ 数据转换成 `MeshTriangle`
4. `MeshTriangle` 内部再拆成很多 `Triangle`
5. 如果开启 BVH，则为网格或场景建立加速结构
6. 渲染时，`scene/Scene.cpp` 调用 `intersect()` 向几何模块查询命中结果

## `Object` 接口为什么重要

`Object` 是这个模块的核心抽象。它要求所有几何体都能提供：

- `getIntersection()`：光线相交
- `getSurfaceProperties()`：命中后的法线、纹理坐标等表面属性
- `evalDiffuseColor()`：默认颜色或贴图颜色
- `getBounds()`：包围盒
- `getArea()`：面积
- `Sample()`：按面积采样一点
- `hasEmit()`：是否自发光

这也是为什么：

- 三角形可以被当作普通表面
- 自发光网格可以被当作灯光采样
- BVH 可以统一处理不同几何体

## 当前支持的几何类型

### `Triangle`

- 最基础的面片原语
- 负责三角形级别的相交、面积和表面属性计算
- 也是 OBJ 网格最终拆分后的基本单元

### `MeshTriangle`

- 用来承载一个 OBJ 模型
- 内部保存很多 `Triangle`
- 会根据子网格分配对应材质
- 支持导入时应用 `offset`、`rotation`、`scale`
- 支持自发光网格统计和采样

### `Sphere`

- 主要用于简单原语和可见球形面积灯模型

## 在 `.scene` 里怎么新增模型

最常见的方式是写一个 `mesh` 块：

```text
mesh "table/table.obj" {
    materialMap = "table/table.materialmap"
    offset      = 0.000 0.000 -0.008
    rotation    = 0.000 0.000 0.000
    scale       = 1.0 1.0 1.0
}
```

常见字段：

- `material`：整模型统一材质类型
- `materialMap`：按 MTL 子材质名分配内部材质
- `offset`：平移
- `rotation`：欧拉角旋转
- `scale`：缩放

通常推荐：

- 简单模型测试时直接写 `material`
- 复杂 OBJ 场景优先写 `materialMap`

## 想删除一个模型怎么做

最简单的方式不是改几何代码，而是：

- 直接从 `.scene` 删除对应的 `mesh` 块

如果只是临时不想显示某个模型：

- 也可以把它从场景文件移走，或者单独做一个测试场景

## 想修改模型摆放怎么做

一般不需要动 `geometry/` 代码，直接改 `.scene`：

- 改 `offset`
- 改 `rotation`
- 改 `scale`

这种改法最适合做：

- 构图微调
- 桌面布景
- 产品展示场景

## 想新增一种几何原语怎么做

如果你想新增一个比如：

- 平面
- 圆柱
- 胶囊体
- 程序生成的盒子

通常流程是：

1. 新建一个继承 `Object` 的类
2. 实现相交、包围盒、面积、采样等接口
3. 在 `scene/SceneLoader.cpp` 增加新的解析入口，或者复用已有 `light` / `mesh` 路径
4. 确保它能正确携带 `Material*`

如果这个新几何还要支持自发光：

- 记得正确实现 `hasEmit()`、`getArea()` 和 `Sample()`

## 这个模块和材质、灯光怎么配合

### 和材质

几何体本身不决定“看起来像什么”，它只决定：

- 命中到了哪
- 表面朝向哪
- 纹理坐标是多少

真正的外观由 `material/` 负责。

### 和灯光

如果一个物体的材质带有 emission：

- `geometry/` 提供面积与采样能力
- `light/` 可以把它适配成 `MeshLight`

所以自发光网格其实是几何模块和灯光模块共同完成的。

## 需要注意的限制

- OBJ 导入是这个模块当前最主要的资产入口
- 大型网格虽然支持 BVH，但导入与预处理仍会占用一定时间
- 场景里摆位靠 `.scene`，不是靠在代码里手改顶点

## 协作时建议怎么分工

这个模块适合分成几类任务：

- 一人负责 OBJ / 网格导入
- 一人负责新原语或采样接口
- 一人负责 BVH / 加速结构

但改 `Object` 接口时要特别慎重，因为它会影响：

- `scene/`
- `light/`
- `renderer/`
- 所有具体几何体实现
