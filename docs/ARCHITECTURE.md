# ImuxCarft architecture

C++ is the authoritative native engine layer. GLSL contains GPU programs. C# is the high-level gameplay/tooling layer and will use a versioned C ABI rather than a C++ ABI. Rust is isolated to modules where its ownership model provides a clear benefit; it must not become a second competing engine.

The initial renderer is OpenGL 3.3. Renderer-facing interfaces are backend-independent so a Vulkan backend can be introduced later.

Voxel rendering will follow:
World data → dirty chunk queue → meshing worker → GPU mesh upload → culling → draw.

The engine should avoid per-block heap allocations and keep active chunks bounded by render distance. Cross-language calls should stay at coarse-grained boundaries, not in per-block/per-frame hot loops.
