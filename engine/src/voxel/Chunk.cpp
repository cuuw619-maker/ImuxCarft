#include "imux/voxel/Chunk.hpp"
#include <algorithm>
#include <cassert>
namespace imux::voxel {
Block& Chunk::at(int x,int y,int z) noexcept { assert(x>=0&&x<ChunkSizeX&&y>=0&&y<ChunkSizeY&&z>=0&&z<ChunkSizeZ); return blocks_[index(x,y,z)]; }
const Block& Chunk::at(int x,int y,int z) const noexcept { assert(x>=0&&x<ChunkSizeX&&y>=0&&y<ChunkSizeY&&z>=0&&z<ChunkSizeZ); return blocks_[index(x,y,z)]; }
void Chunk::fill(Block b) noexcept { std::fill(blocks_.begin(),blocks_.end(),b); }
void Chunk::generateFlat(int groundHeight) noexcept {
 groundHeight=std::max(1,std::min(groundHeight,ChunkSizeY-1));
 for(int y=0;y<ChunkSizeY;y++) for(int z=0;z<ChunkSizeZ;z++) for(int x=0;x<ChunkSizeX;x++){
  BlockType t=BlockType::Air;
  if(y<groundHeight-3)t=BlockType::Stone; else if(y<groundHeight-1)t=BlockType::Dirt; else if(y<groundHeight)t=BlockType::Grass;
  at(x,y,z).id=static_cast<BlockId>(t);
 }
}
}