#pragma once
#include "imux/voxel/Block.hpp"
#include <array>
namespace imux::voxel {
inline constexpr int ChunkSizeX=16, ChunkSizeY=128, ChunkSizeZ=16;
class Chunk {
public:
    Block& at(int x,int y,int z) noexcept;
    const Block& at(int x,int y,int z) const noexcept;
    void fill(Block block) noexcept;
private:
    static constexpr std::size_t index(int x,int y,int z) noexcept {
        return static_cast<std::size_t>(x + ChunkSizeX * (z + ChunkSizeZ * y));
    }
    std::array<Block, ChunkSizeX*ChunkSizeY*ChunkSizeZ> blocks_{};
};
}
