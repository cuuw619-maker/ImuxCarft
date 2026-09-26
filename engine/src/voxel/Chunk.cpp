#include "imux/voxel/Chunk.hpp"
#include <algorithm>
#include <cassert>
namespace imux::voxel {
Block& Chunk::at(int x,int y,int z) noexcept { assert(x>=0&&x<ChunkSizeX&&y>=0&&y<ChunkSizeY&&z>=0&&z<ChunkSizeZ); return blocks_[index(x,y,z)]; }
const Block& Chunk::at(int x,int y,int z) const noexcept { assert(x>=0&&x<ChunkSizeX&&y>=0&&y<ChunkSizeY&&z>=0&&z<ChunkSizeZ); return blocks_[index(x,y,z)]; }
void Chunk::fill(Block b) noexcept { std::fill(blocks_.begin(),blocks_.end(),b); }
}
