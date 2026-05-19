# assets/envmaps

这个目录存放 HDR 环境图，用于图像式光照。

## 使用方式

可以在 `.scene` 中写：

```text
envmap "../../envmaps/typewriter_studio_soft.hdr" {
    intensity = 0.18
}
```

也可以通过 CLI 覆盖。

## 说明

- miss 光线会采样环境图。
- 环境图支持直接采样。
- 环境图采样与 BSDF 采样通过 MIS 结合。
