#pragma once
#include <cstdint>
namespace imux::voxel {
using BlockId=std::uint16_t;
enum class BlockType:BlockId { Air=0, Grass=1, Dirt=2, Stone=3 };
struct Block { BlockId id=static_cast<BlockId>(BlockType::Air); constexpr bool isAir() const noexcept{return id==0;} };
constexpr bool isSolid(BlockId id) noexcept{return id!=static_cast<BlockId>(BlockType::Air);}
}