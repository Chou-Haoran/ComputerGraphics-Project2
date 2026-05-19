# material 模块

这个模块负责“表面看起来像什么”。它不仅决定颜色，还决定：

- 光是怎么被反射的
- 是漫反射还是镜面反射
- 是金属、玻璃还是自发光
- 贴图、法线贴图、粗糙度贴图如何接线

如果说：

- `geometry/` 决定“物体在哪里”
- `light/` 决定“光从哪里来”

那么 `material/` 决定的就是：

- “这些物体表面对光会做什么”

## 主要文件

- `Material.hpp` / `Material.cpp`：材质基类、公共参数、共享的 Fresnel / GGX / 贴图逻辑
- `MaterialFactory.hpp` / `MaterialFactory.cpp`：按字符串创建材质的工厂与自注册机制
- `DiffuseMaterial.cpp`
- `MicrofacetMaterial.cpp`
- `MetalMaterial.cpp`
- `MirrorMaterial.cpp`
- `GlassMaterial.cpp`
- `EmitMaterial.cpp`
- `Texture.hpp` / `Texture.cpp`：贴图加载与采样

## 当前支持的材质类型

- `DIFFUSE`
- `MICROFACET`
- `METAL`
- `MIRROR`
- `GLASS`
- `EMIT`

## 每种材质大概适合什么

### `DIFFUSE`

- 纯漫反射
- 适合墙面、纸张、哑光塑料、简单测试

### `MICROFACET`

- 漫反射 + 微表面镜面
- 适合塑料、喷漆、皮革、纸、非金属涂层
- 是最常用、最灵活的一类

### `METAL`

- 以 GGX 镜面为主
- 适合铬、钢、黄铜、拉丝金属

### `MIRROR`

- 理想镜面反射
- 更偏“算法测试”或非常强的镜面效果

### `GLASS`

- 反射 + 折射的 delta 材质
- 适合玻璃、透明外壳、镜片

### `EMIT`

- 自发光材质
- 既能直接在画面中发光，也能被转换成 `MeshLight`

## 材质是怎么分配到模型上的

这是这个项目里最需要讲清楚的一部分。

### 方式 1：整模型直接指定一种材质

在 `.scene` 的 `mesh { ... }` 里写：

```text
mesh "my_prop/prop.obj" {
    material = MICROFACET
    color    = 0.7 0.7 0.7
    roughness = 0.25
}
```

这种方式会让整个模型统一使用一种内部材质。

适合：

- 快速测试
- 单材质模型
- 临时对比不同材质效果

### 方式 2：通过 `.materialmap` 按 OBJ/MTL 子材质名映射

对于复杂模型，更推荐：

```text
mesh "typewriter/typewriter.obj" {
    materialMap = "typewriter/typewriter.materialmap"
}
```

然后在 `.materialmap` 里写：

```text
materialMap {
    defaultType = MICROFACET
    defaultRoughnessMin = 0.18
}

mtl "Metal_Chrome" {
    type = METAL
    color = 0.85 0.85 0.84
    roughness = 0.10
    Ks = 0.95
}
```

这里的核心思想是：

- OBJ/MTL 原始材质名保留不动
- 项目内部真正使用的材质类型和参数由 `.materialmap` 接管

这样比直接信任原始 `.mtl` 更可控，也更适合协作。

### 方式 3：让加载器自动推断

如果你既没写 `material`，也没写 `materialMap`，加载器会尝试根据：

- MTL 名称
- `Kd` / `Ks`
- `Ns`
- 透明度
- 贴图存在情况

自动推断材质类型。

这适合快速导入，但不适合最终展示，因为：

- 推断只是经验规则
- 不是绝对可靠

## 常见可调参数

无论是 `.scene` 直接写，还是 `.materialmap` 里写，常见参数包括：

- `color`
- `roughness`
- `ior`
- `Kd`
- `Ks`
- `emission`
- `diffuseTex`
- `normalTex`
- `bumpTex`
- `roughnessTex`
- `roughnessTexIsGloss`
- `bumpScale`

### 参数大概怎么理解

- `color`：基础颜色
- `roughness`：表面粗糙度，越小越锐利
- `ior`：折射率，主要给玻璃用
- `Kd`：漫反射权重
- `Ks`：镜面权重
- `emission`：自发光颜色与强度基础值
- `diffuseTex`：漫反射贴图
- `normalTex`：法线贴图
- `bumpTex`：凹凸贴图
- `roughnessTex`：粗糙度贴图

## 贴图是怎么工作的

当前贴图链路支持：

- 漫反射颜色贴图
- 法线贴图
- 凹凸贴图
- 粗糙度贴图

其中：

- `diffuseTexture` 会和 `color` 相乘
- `normalTexture` 会在 shading 阶段改写法线
- `bumpTexture` 会进一步扰动局部法线
- `roughnessTexture` 会调节最终 roughness

如果某个 OBJ 本身 MTL 里带贴图，而你没手动覆盖：

- `SceneLoader` 会尽量自动接上

## 常见操作手册

### 想把一个部件改成金属

优先改 `.materialmap`，例如：

```text
mtl "Metal_Chrome" {
    type = METAL
    color = 0.85 0.85 0.84
    roughness = 0.08
    Ks = 0.95
}
```

### 想把纸张改得更明显

通常用：

- `MICROFACET` 或 `DIFFUSE`
- 稍亮的 `color`
- 较高的 `roughness`
- 较低的 `Ks`

### 想让某个模型发光

可以在 `.scene` 的 `mesh` 上直接写：

```text
emission = 10.0 9.5 9.0
```

或者在 `.materialmap` 里给某个 `mtl` 条目写 `emission`。

### 想删除某种材质分配

如果是场景里直接指定的：

- 从 `.scene` 删除对应的 `material = ...` 或贴图字段

如果是 `materialMap` 做的：

- 从 `.materialmap` 删除对应 `mtl "..." { ... }` 条目

如果删除后没有别的覆盖规则：

- 它会退回默认类型或推断类型

## 想新增一种材质怎么做

这是当前工程化改造后最值得利用的部分。

当前材质系统不是靠一个中心大 `switch`，而是靠：

- `Material` 抽象基类
- `MaterialFactory`
- `MaterialRegistrar<T>`

所以新增材质的标准流程是：

1. 新建一个 `xxxMaterial.cpp`
2. 定义一个继承 `Material` 的类
3. 实现：
   - `typeName()`
   - `sample()`
   - `pdf()`
   - `eval()`
   - 如有需要，覆写 `resolveInteraction()`
4. 在文件末尾注册：

```cpp
const MaterialRegistrar<MyMaterial> kMyMaterialRegistrar("MY_MATERIAL");
```

5. 重新编译后，就可以在 `.scene` 或 `.materialmap` 里直接写：

```text
type = MY_MATERIAL
```

## 想删除一种材质怎么做

删除材质类型时不能只删一个文件，要一起检查：

1. 对应的 `xxxMaterial.cpp`
2. 任何场景文件或 `.materialmap` 是否仍引用它
3. README / 文档是否仍列出它

如果场景里还写着这个类型名，而代码里已经删掉：

- `SceneLoader` 会在解析时报 `unknown material type`

所以“删除材质”的正确顺序通常是：

1. 先把场景改掉
2. 再删实现文件
3. 最后更新文档

## 想修改某个材质实现的入口在哪

- 想改共享的 GGX / Fresnel / 法线贴图逻辑：看 `Material.cpp`
- 想改微表面非金属：看 `MicrofacetMaterial.cpp`
- 想改金属：看 `MetalMaterial.cpp`
- 想改镜面：看 `MirrorMaterial.cpp`
- 想改玻璃：看 `GlassMaterial.cpp`
- 想改自发光：看 `EmitMaterial.cpp`

## 当前实现上需要知道的事实

- `MICROFACET` 已经不是固定 50/50 采样，而是更偏自适应的镜面/漫反射混合
- `METAL` 使用 GGX 反射采样
- `EMIT` 会影响画面可见发光，也可能被作为灯光采样
- 法线贴图和 bump 贴图会共同影响最终 shading normal

## 协作时怎么分工

这个模块很适合多人并行：

- 一人做新材质
- 一人做贴图支持
- 一人做 `.materialmap` 与场景调参

因为材质已经按文件拆开，增删改不需要再回一个中心枚举文件里维护大块逻辑。
