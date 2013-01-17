#include "common.h"
#include "SegmentProcessing.h"
#include "OcclusionTest.h"
#include "ContentLoader.h"
#include "GameBuildings.h"
#include "SpriteMaps.h"

//big look up table
uint8_t rampblut[] =
    // generated by blutmaker.py
{
    1 ,  2 ,  8 ,  2 ,  4 , 12 ,  4 , 12 ,  9 ,  2 , 21 ,  2 ,  4 , 12 ,  4 , 12 ,
    5 , 16 ,  5 , 16 , 13 , 13 , 13 , 12 ,  5 , 16 ,  5 , 16 , 13 , 13 , 13 , 16 ,
    7 ,  2 , 14 ,  2 ,  4 , 12 ,  4 , 12 , 20 , 26 , 25 , 26 ,  4 , 12 ,  4 , 12 ,
    5 , 16 ,  5 , 16 , 13 , 16 , 13 , 16 ,  5 , 16 ,  5 , 16 , 13 , 16 , 13 , 16 ,
    3 , 10 ,  3 , 10 , 17 , 12 , 17 , 12 ,  3 , 10 , 26 , 10 , 17 , 17 , 17 , 17 ,
    11 , 10 , 11 , 16 , 11 , 26 , 17 , 12 , 11 , 16 , 11 , 16 , 13 , 13 , 17 , 16 ,
    3 , 10 ,  3 , 10 , 17 , 17 , 17 , 17 ,  3 , 10 , 26 , 10 , 17 , 17 , 17 , 17 ,
    11 , 11 , 11 , 16 , 11 , 11 , 17 , 14 , 11 , 16 , 11 , 16 , 17 , 17 , 17 , 13 ,
    6 ,  2 , 19 ,  2 ,  4 , 12 ,  4 , 12 , 15 ,  2 , 24 ,  2 ,  4 , 12 ,  4 , 12 ,
    5 , 16 , 26 , 16 , 13 , 16 , 13 , 16 ,  5 , 16 , 26 , 16 , 13 , 16 , 13 , 16 ,
    18 ,  2 , 22 ,  2 , 26 , 12 , 26 , 12 , 23 , 26 , 26 , 26 , 26 , 12 , 26 , 12 ,
    5 , 16 , 26 , 16 , 13 , 16 , 13 , 16 ,  5 , 16 , 26 , 16 , 13 , 16 , 13 , 16 ,
    3 , 10 ,  3 , 10 , 17 , 10 , 17 , 17 ,  3 , 10 , 26 , 10 , 17 , 17 , 17 , 17 ,
    11 , 10 , 11 , 16 , 17 , 10 , 17 , 17 , 11 , 16 , 11 , 16 , 17 , 15 , 17 , 12 ,
    3 , 10 ,  3 , 10 , 17 , 17 , 17 , 17 ,  3 , 10 , 26 , 10 , 17 , 17 , 17 , 17 ,
    11 , 16 , 11 , 16 , 17 , 16 , 17 , 10 , 11 , 16 , 11 , 16 , 17 , 11 , 17 , 26
};

inline bool isTileHighRampEnd(uint32_t x, uint32_t y, uint32_t z, WorldSegment* segment, dirRelative dir)
{
    Tile* tile = segment->getTileRelativeTo( x, y, z, dir);
    if(!tile) {
        return false;
    }
    if(tile->tileShapeBasic()!=tiletype_shape_basic::Wall) {
        return false;
    }
    return IDisWall( tile->tileType );
}

inline int tileWaterDepth(uint32_t x, uint32_t y, uint32_t z, WorldSegment* segment, dirRelative dir)
{
    Tile* tile = segment->getTileRelativeTo( x, y, z, dir);
    if(!tile) {
        return false;
    }
    if(tile->designation.bits.flow_size == 0 || tile->designation.bits.liquid_type == 1) {
        return false;
    }
    return tile->designation.bits.flow_size;
}

inline bool isTileHighRampTop(uint32_t x, uint32_t y, uint32_t z, WorldSegment* segment, dirRelative dir)
{
    Tile* tile = segment->getTileRelativeTo( x, y, z, dir);
    if(!tile) {
        return false;
    }
    if(tile->tileShapeBasic()!=tiletype_shape_basic::Floor 
        && tile->tileShapeBasic()!=tiletype_shape_basic::Ramp 
        && tile->tileShapeBasic()!=tiletype_shape_basic::Stair) {
            return false;
    }
    if(tile->tileShapeBasic()!=tiletype_shape_basic::Wall) {
        return true;
    }
    return !IDisWall( tile->tileType );
}

uint8_t CalculateRampType(uint32_t x, uint32_t y, uint32_t z, WorldSegment* segment)
{
    int32_t ramplookup = 0;
    if (isTileHighRampEnd(x, y, z, segment, eUp) && isTileHighRampTop(x, y, z+1, segment, eUp)) {
        ramplookup ^= 1;
    }
    if (isTileHighRampEnd(x, y, z, segment, eUpRight) && isTileHighRampTop(x, y, z+1, segment, eUpRight)) {
        ramplookup ^= 2;
    }
    if (isTileHighRampEnd(x, y, z, segment, eRight) && isTileHighRampTop(x, y, z+1, segment, eRight)) {
        ramplookup ^= 4;
    }
    if (isTileHighRampEnd(x, y, z, segment, eDownRight) && isTileHighRampTop(x, y, z+1, segment, eDownRight)) {
        ramplookup ^= 8;
    }
    if (isTileHighRampEnd(x, y, z, segment, eDown) && isTileHighRampTop(x, y, z+1, segment, eDown)) {
        ramplookup ^= 16;
    }
    if (isTileHighRampEnd(x, y, z, segment, eDownLeft) && isTileHighRampTop(x, y, z+1, segment, eDownLeft)) {
        ramplookup ^= 32;
    }
    if (isTileHighRampEnd(x, y, z, segment, eLeft) && isTileHighRampTop(x, y, z+1, segment, eLeft)) {
        ramplookup ^= 64;
    }
    if (isTileHighRampEnd(x, y, z, segment, eUpLeft) && isTileHighRampTop(x, y, z+1, segment, eUpLeft)) {
        ramplookup ^= 128;
    }

    // creation should ensure in range
    if (ramplookup > 0) {
        return rampblut[ramplookup];
    }

    if (isTileHighRampEnd(x, y, z, segment, eUp)) {
        ramplookup ^= 1;
    }
    if (isTileHighRampEnd(x, y, z, segment, eUpRight)) {
        ramplookup ^= 2;
    }
    if (isTileHighRampEnd(x, y, z, segment, eRight)) {
        ramplookup ^= 4;
    }
    if (isTileHighRampEnd(x, y, z, segment, eDownRight)) {
        ramplookup ^= 8;
    }
    if (isTileHighRampEnd(x, y, z, segment, eDown)) {
        ramplookup ^= 16;
    }
    if (isTileHighRampEnd(x, y, z, segment, eDownLeft)) {
        ramplookup ^= 32;
    }
    if (isTileHighRampEnd(x, y, z, segment, eLeft)) {
        ramplookup ^= 64;
    }
    if (isTileHighRampEnd(x, y, z, segment, eUpLeft)) {
        ramplookup ^= 128;
    }

    // creation should ensure in range
    return rampblut[ramplookup];
}

bool checkFloorBorderRequirement(WorldSegment* segment, int x, int y, int z, dirRelative offset)
{
    Tile* bHigh = segment->getTileRelativeTo(x, y, z, offset);
    if (bHigh && 
        (bHigh->tileShapeBasic()==tiletype_shape_basic::Floor 
        || bHigh->tileShapeBasic()==tiletype_shape_basic::Ramp 
        || bHigh->tileShapeBasic()==tiletype_shape_basic::Wall)) {
            return false;
    }
    Tile* bLow = segment->getTileRelativeTo(x, y, z-1, offset);
    if (bLow == NULL || bLow->tileShapeBasic()!=tiletype_shape_basic::Ramp) {
        return true;
    }
    return false;
}

bool isTileOnVisibleEdgeOfSegment(WorldSegment* segment, Tile* b)
{
    if(b->z == segment->pos.z + segment->size.z - 2) {
        return true;
    }

    if (ssState.DisplayedRotation == 0 &&
        (
        b->x == segment->pos.x + segment->size.x - 2
        || b->y == segment->pos.y + segment->size.y - 2
        || b->x == segment->regionSize.x - 1
        || b->y == segment->regionSize.y - 1
        )) {
            return true;
    } else if (ssState.DisplayedRotation == 1 &&
        (
        b->x == segment->pos.x + segment->size.y - 2
        || b->y == segment->pos.y + 1
        || b->x == segment->regionSize.x - 1
        || b->y == 0
        )) {
            return true;
    } else if (ssState.DisplayedRotation == 2 &&
        (
        b->x == segment->pos.x + 1
        || b->y == segment->pos.y + 1
        || b->x == 0
        || b->y == 0
        )) {
            return true;
    } else if (ssState.DisplayedRotation == 3 &&
        (
        b->x == segment->pos.x + 1
        || b->y == segment->pos.y + segment->size.x - 2
        || b->x == 0
        || b->y == segment->regionSize.y - 1
        )) {
            return true;
    }

    return false;
}

bool areNeighborsVisible(WorldSegment* segment, Tile* b)
{
    Tile* temp;

    temp = segment->getTile(b->x, b->y, b->z+1);
    if(!temp || !(temp->designation.bits.hidden)) {
        return true;
    }

    temp = segment->getTile(b->x+1, b->y, b->z);
    if(!temp || !(temp->designation.bits.hidden)) {
        return true;
    }
    temp = segment->getTile(b->x-1, b->y, b->z);
    if(!temp || !(temp->designation.bits.hidden)) {
        return true;
    }
    temp = segment->getTile(b->x, b->y+1, b->z);
    if(!temp || !(temp->designation.bits.hidden)) {
        return true;
    }
    temp = segment->getTile(b->x, b->y-1, b->z);
    if(!temp || !(temp->designation.bits.hidden)) {
        return true;
    }
    return false;
}

/**
* returns true iff the tile is enclosed by other solid tiles, and is itself solid
*/
bool enclosed(WorldSegment* segment, Tile* b)
{
    if(!IDisWall(b->tileType)) {
        return false;
    }

    Tile* temp;
    temp = segment->getTile(b->x, b->y, b->z+1);
    if(!temp || !IDhasOpaqueFloor(temp->tileType)) {
        return false;
    }

    temp = segment->getTile(b->x+1, b->y, b->z);
    if(!temp || !IDisWall(temp->tileType)) {
        return false;
    }
    temp = segment->getTile(b->x-1, b->y, b->z);
    if(!temp || !IDisWall(temp->tileType)) {
        return false;
    }
    temp = segment->getTile(b->x, b->y+1, b->z);
    if(!temp || !IDisWall(temp->tileType)) {
        return false;
    }
    temp = segment->getTile(b->x, b->y-1, b->z);
    if(!temp || !IDisWall(temp->tileType)) {
        return false;
    }

    return true;
}

/**
* checks to see if the tile is a potentially viewable hidden tile
*  if so, put the black mask tile overtop
*  if not, makes tile not visible
*/
inline void maskTile(WorldSegment * segment, Tile* b)
{
    //include hidden tiles as shaded black, make remaining invisible
    if( b->designation.bits.hidden ) {
        if( isTileOnVisibleEdgeOfSegment(segment, b)
            || areNeighborsVisible(segment, b) ) {
                b->building.type = (building_type::building_type) BUILDINGTYPE_BLACKBOX;
        } else {
            b->visible = false;
        }
    }
}

/**
* checks to see if the tile is a potentially viewable hidden tile
*  if not, makes tile not visible
* ASSUMES YOU ARE NOT ON THE SEGMENT EDGE
*/
inline void enclosedTile(WorldSegment * segment, Tile* b)
{
    //make tiles that are impossible to see invisible
    if( b->designation.bits.hidden
        && (enclosed(segment, b)) ) {
            b->visible = false;
    }
}

/**
* enables visibility and disables fog for the first layer of water
*  below visible space
*/
inline void unhideWaterFromAbove(WorldSegment * segment, Tile * b)
{
    if( b->designation.bits.flow_size
        && !isTileOnTopOfSegment(segment, b)
        && (b->designation.bits.hidden || b->fog_of_war) ) {
            Tile * temp = segment->getTile(b->x, b->y, b->z+1);
            if( !temp || (!IDhasOpaqueFloor(temp->tileType) && !temp->designation.bits.flow_size) ) {
                if(contentLoader->gameMode.g_mode == GAMEMODE_ADVENTURE) {
                    if(!temp || !temp->fog_of_war) {
                        b->designation.bits.hidden = false;
                        b->fog_of_war = false;
                        if(b->building.type == BUILDINGTYPE_BLACKBOX) {
                            b->building.type = (building_type::building_type) BUILDINGTYPE_NA;
                        }
                    }
                } else {
                    if(!temp || !temp->designation.bits.hidden) {
                        b->designation.bits.hidden = false;
                        if(b->building.type == BUILDINGTYPE_BLACKBOX) {
                            b->building.type = (building_type::building_type) BUILDINGTYPE_NA;
                        }
                    }
                }
            }
    }
}

/**
 * sets up the tile border conditions of the given block
 */
void arrangeTileBorders(WorldSegment * segment, Tile* b)
{

    //add edges to tiles and floors
    Tile * dir1 = segment->getTileRelativeTo(b->x, b->y, b->z, eUpLeft);
    Tile * dir2 = segment->getTileRelativeTo(b->x, b->y, b->z, eUp);
    Tile * dir3 = segment->getTileRelativeTo(b->x, b->y, b->z, eUpRight);
    Tile * dir4 = segment->getTileRelativeTo(b->x, b->y, b->z, eRight);
    Tile * dir5 = segment->getTileRelativeTo(b->x, b->y, b->z, eDownRight);
    Tile * dir6 = segment->getTileRelativeTo(b->x, b->y, b->z, eDown);
    Tile * dir7 = segment->getTileRelativeTo(b->x, b->y, b->z, eDownLeft);
    Tile * dir8 = segment->getTileRelativeTo(b->x, b->y, b->z, eLeft);

    b->obscuringBuilding=0;
    b->obscuringCreature=0;

    if(dir1) if(dir1->occ.bits.unit) {
        b->obscuringCreature = 1;
    }
    if(dir2) if(dir2->occ.bits.unit) {
        b->obscuringCreature = 1;
    }
    if(dir8) if(dir8->occ.bits.unit) {
        b->obscuringCreature = 1;
    }

    if(dir1)
        if(dir1->building.type != BUILDINGTYPE_NA
            && dir1->building.type != BUILDINGTYPE_BLACKBOX
            && dir1->building.type != df::enums::building_type::Civzone
            && dir1->building.type != df::enums::building_type::Stockpile
            ) {
                b->obscuringBuilding = 1;
        }
        if(dir2)
            if(dir2->building.type != BUILDINGTYPE_NA
                && dir2->building.type != BUILDINGTYPE_BLACKBOX
                && dir2->building.type != df::enums::building_type::Civzone
                && dir2->building.type != df::enums::building_type::Stockpile
                ) {
                    b->obscuringBuilding = 1;
            }
            if(dir8)
                if(dir8->building.type != BUILDINGTYPE_NA
                    && dir8->building.type != BUILDINGTYPE_BLACKBOX
                    && dir8->building.type != df::enums::building_type::Civzone
                    && dir8->building.type != df::enums::building_type::Stockpile
                    ) {
                        b->obscuringBuilding = 1;
                }

                if( b->tileShapeBasic()==tiletype_shape_basic::Floor ) {
                    b->depthBorderWest = checkFloorBorderRequirement(segment, b->x, b->y, b->z, eLeft);
                    b->depthBorderNorth = checkFloorBorderRequirement(segment, b->x, b->y, b->z, eUp);

                    Tile* belowTile = segment->getTileRelativeTo(b->x, b->y, b->z, eBelow);
                    if(!belowTile || (belowTile->tileShapeBasic()!=tiletype_shape_basic::Wall && belowTile->tileShapeBasic()!=tiletype_shape_basic::Wall)) {
                        b->depthBorderDown = true;
                    }
                } else if( b->tileShapeBasic()==tiletype_shape_basic::Wall && wallShouldNotHaveBorders( b->tileType ) == false ) {
                    Tile* leftTile = segment->getTileRelativeTo(b->x, b->y, b->z, eLeft);
                    Tile* upTile = segment->getTileRelativeTo(b->x, b->y, b->z, eUp);
                    if(!leftTile || (leftTile->tileShapeBasic()!=tiletype_shape_basic::Wall && leftTile->tileShapeBasic()!=tiletype_shape_basic::Ramp)) {
                        b->depthBorderWest = true;
                    }
                    if(!upTile || (upTile->tileShapeBasic()!=tiletype_shape_basic::Wall && upTile->tileShapeBasic()!=tiletype_shape_basic::Ramp)) {
                        b->depthBorderNorth = true;
                    }
                    Tile* belowTile = segment->getTileRelativeTo(b->x, b->y, b->z, eBelow);
                    if(!belowTile || (belowTile->tileShapeBasic()!=tiletype_shape_basic::Wall && belowTile->tileShapeBasic()!=tiletype_shape_basic::Ramp)) {
                        b->depthBorderDown = true;
                    }
                }
                b->wallborders = 0;
                if(dir1) if(dir1->tileShapeBasic()==tiletype_shape_basic::Wall) {
                    b->wallborders |= 1;
                }
                if(dir2) if(dir2->tileShapeBasic()==tiletype_shape_basic::Wall) {
                    b->wallborders |= 2;
                }
                if(dir3) if(dir3->tileShapeBasic()==tiletype_shape_basic::Wall) {
                    b->wallborders |= 4;
                }
                if(dir4) if(dir4->tileShapeBasic()==tiletype_shape_basic::Wall) {
                    b->wallborders |= 8;
                }
                if(dir5) if(dir5->tileShapeBasic()==tiletype_shape_basic::Wall) {
                    b->wallborders |= 16;
                }
                if(dir6) if(dir6->tileShapeBasic()==tiletype_shape_basic::Wall) {
                    b->wallborders |= 32;
                }
                if(dir7) if(dir7->tileShapeBasic()==tiletype_shape_basic::Wall) {
                    b->wallborders |= 64;
                }
                if(dir8) if(dir8->tileShapeBasic()==tiletype_shape_basic::Wall) {
                    b->wallborders |= 128;
                }

                b->rampborders = 0;
                if(dir1) if(dir1->tileShapeBasic()==tiletype_shape_basic::Ramp) {
                    b->wallborders |= 1;
                }
                if(dir2) if(dir2->tileShapeBasic()==tiletype_shape_basic::Ramp) {
                    b->wallborders |= 2;
                }
                if(dir3) if(dir3->tileShapeBasic()==tiletype_shape_basic::Ramp) {
                    b->wallborders |= 4;
                }
                if(dir4) if(dir4->tileShapeBasic()==tiletype_shape_basic::Ramp) {
                    b->wallborders |= 8;
                }
                if(dir5) if(dir5->tileShapeBasic()==tiletype_shape_basic::Ramp) {
                    b->wallborders |= 16;
                }
                if(dir6) if(dir6->tileShapeBasic()==tiletype_shape_basic::Ramp) {
                    b->wallborders |= 32;
                }
                if(dir7) if(dir7->tileShapeBasic()==tiletype_shape_basic::Ramp) {
                    b->wallborders |= 64;
                }
                if(dir8) if(dir8->tileShapeBasic()==tiletype_shape_basic::Ramp) {
                    b->wallborders |= 128;
                }

                b->upstairborders = 0;
                b->downstairborders = 0;
                if(dir1) if(dir1->tileShape() == tiletype_shape::STAIR_UP) {
                    b->upstairborders |= 1;
                }
                if(dir2) if(dir2->tileShape() == tiletype_shape::STAIR_UP) {
                    b->upstairborders |= 2;
                }
                if(dir3) if(dir3->tileShape() == tiletype_shape::STAIR_UP) {
                    b->upstairborders |= 4;
                }
                if(dir4) if(dir4->tileShape() == tiletype_shape::STAIR_UP) {
                    b->upstairborders |= 8;
                }
                if(dir5) if(dir5->tileShape() == tiletype_shape::STAIR_UP) {
                    b->upstairborders |= 16;
                }
                if(dir6) if(dir6->tileShape() == tiletype_shape::STAIR_UP) {
                    b->upstairborders |= 32;
                }
                if(dir7) if(dir7->tileShape() == tiletype_shape::STAIR_UP) {
                    b->upstairborders |= 64;
                }
                if(dir8) if(dir8->tileShape() == tiletype_shape::STAIR_UP) {
                    b->upstairborders |= 128;
                }

                if(dir1) if(dir1->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->upstairborders |= 1;
                }
                if(dir2) if(dir2->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->upstairborders |= 2;
                }
                if(dir3) if(dir3->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->upstairborders |= 4;
                }
                if(dir4) if(dir4->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->upstairborders |= 8;
                }
                if(dir5) if(dir5->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->upstairborders |= 16;
                }
                if(dir6) if(dir6->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->upstairborders |= 32;
                }
                if(dir7) if(dir7->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->upstairborders |= 64;
                }
                if(dir8) if(dir8->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->upstairborders |= 128;
                }

                if(dir1) if(dir1->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->downstairborders |= 1;
                }
                if(dir2) if(dir2->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->downstairborders |= 2;
                }
                if(dir3) if(dir3->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->downstairborders |= 4;
                }
                if(dir4) if(dir4->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->downstairborders |= 8;
                }
                if(dir5) if(dir5->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->downstairborders |= 16;
                }
                if(dir6) if(dir6->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->downstairborders |= 32;
                }
                if(dir7) if(dir7->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->downstairborders |= 64;
                }
                if(dir8) if(dir8->tileShape() == tiletype_shape::STAIR_UPDOWN) {
                    b->downstairborders |= 128;
                }

                if(dir1) if(dir1->tileShape() == tiletype_shape::STAIR_DOWN) {
                    b->downstairborders |= 1;
                }
                if(dir2) if(dir2->tileShape() == tiletype_shape::STAIR_DOWN) {
                    b->downstairborders |= 2;
                }
                if(dir3) if(dir3->tileShape() == tiletype_shape::STAIR_DOWN) {
                    b->downstairborders |= 4;
                }
                if(dir4) if(dir4->tileShape() == tiletype_shape::STAIR_DOWN) {
                    b->downstairborders |= 8;
                }
                if(dir5) if(dir5->tileShape() == tiletype_shape::STAIR_DOWN) {
                    b->downstairborders |= 16;
                }
                if(dir6) if(dir6->tileShape() == tiletype_shape::STAIR_DOWN) {
                    b->downstairborders |= 32;
                }
                if(dir7) if(dir7->tileShape() == tiletype_shape::STAIR_DOWN) {
                    b->downstairborders |= 64;
                }
                if(dir8) if(dir8->tileShape() == tiletype_shape::STAIR_DOWN) {
                    b->downstairborders |= 128;
                }

                b->floorborders = 0;
                if(dir1) if(dir1->tileShapeBasic()==tiletype_shape_basic::Floor) {
                    b->floorborders |= 1;
                }
                if(dir2) if(dir2->tileShapeBasic()==tiletype_shape_basic::Floor) {
                    b->floorborders |= 2;
                }
                if(dir3) if(dir3->tileShapeBasic()==tiletype_shape_basic::Floor) {
                    b->floorborders |= 4;
                }
                if(dir4) if(dir4->tileShapeBasic()==tiletype_shape_basic::Floor) {
                    b->floorborders |= 8;
                }
                if(dir5) if(dir5->tileShapeBasic()==tiletype_shape_basic::Floor) {
                    b->floorborders |= 16;
                }
                if(dir6) if(dir6->tileShapeBasic()==tiletype_shape_basic::Floor) {
                    b->floorborders |= 32;
                }
                if(dir7) if(dir7->tileShapeBasic()==tiletype_shape_basic::Floor) {
                    b->floorborders |= 64;
                }
                if(dir8) if(dir8->tileShapeBasic()==tiletype_shape_basic::Floor) {
                    b->floorborders |= 128;
                }

                b->lightborders = 0;
                if(dir1) if(!dir1->designation.bits.outside) {
                    b->lightborders |= 1;
                }
                if(dir2) if(!dir2->designation.bits.outside) {
                    b->lightborders |= 2;
                }
                if(dir3) if(!dir3->designation.bits.outside) {
                    b->lightborders |= 4;
                }
                if(dir4) if(!dir4->designation.bits.outside) {
                    b->lightborders |= 8;
                }
                if(dir5) if(!dir5->designation.bits.outside) {
                    b->lightborders |= 16;
                }
                if(dir6) if(!dir6->designation.bits.outside) {
                    b->lightborders |= 32;
                }
                if(dir7) if(!dir7->designation.bits.outside) {
                    b->lightborders |= 64;
                }
                if(dir8) if(!dir8->designation.bits.outside) {
                    b->lightborders |= 128;
                }
                b->lightborders = ~b->lightborders;

                b->openborders = ~(b->floorborders|b->rampborders|b->wallborders|b->downstairborders|b->upstairborders);
}

/**
 * add extra sprites for buildings, trees, etc
 */
void addSegmentExtras(WorldSegment * segment)
{

    uint32_t numtiles = segment->getNumTiles();

    for(uint32_t i=0; i < numtiles; i++) {
        Tile* b = segment->getTile(i);

        if(!b || !b->visible) {
            continue;
        }

        if(!ssConfig.show_hidden_tiles && b->designation.bits.hidden) {
            continue;
        }

        //Grass
        if(b->grasslevel > 0 && (
            (b->tileMaterial() == tiletype_material::GRASS_LIGHT) ||
            (b->tileMaterial() == tiletype_material::GRASS_DARK) ||
            (b->tileMaterial() == tiletype_material::GRASS_DEAD) ||
            (b->tileMaterial() == tiletype_material::GRASS_DRY))) {
                c_tile_tree * vegetationsprite = 0;
                vegetationsprite = getVegetationTree(contentLoader->grassConfigs,b->grassmat,true,true);
                if(vegetationsprite) {
                    vegetationsprite->insert_sprites(segment, b->x, b->y, b->z, b);
                }
        }

        //populate trees
        if(b->tree.index) {
            c_tile_tree * Tree = GetTreeVegetation(b->tileShape(), b->tileSpecial(), b->tree.index );
            Tree->insert_sprites(segment, b->x, b->y, b->z, b);
        }

        //setup building sprites
        if( b->building.type != BUILDINGTYPE_NA && b->building.type != BUILDINGTYPE_BLACKBOX) {
            loadBuildingSprites( b);
        }

        //setup deep water
        if( b->designation.bits.flow_size == 7 && b->designation.bits.liquid_type == 0) {
            int topdepth = tileWaterDepth(b->x, b->y, b->z, segment, eAbove);
            if(topdepth) {
                b->deepwater = true;
            }
        }

        //setup ramps
        if(b->tileShapeBasic()==tiletype_shape_basic::Ramp) {
            b->rampindex = CalculateRampType(b->x, b->y, b->z, segment);
        }

        //setup tile borders
        arrangeTileBorders(segment, b);
    }
}

void optimizeSegment(WorldSegment * segment)
{
    //do misc beautification

    uint32_t numtiles = segment->getNumTiles();

    for(uint32_t i=0; i < numtiles; i++) {
        Tile* b = segment->getTile(i);

        if(!b) {
            continue;
        }

        //try to mask away tiles that are flagged hidden
        if(!ssConfig.show_hidden_tiles ) {
            //unhide any liquids that are visible from above
            unhideWaterFromAbove(segment, b);
            if(ssConfig.shade_hidden_tiles) {
                maskTile(segment, b);
            } else if( b->designation.bits.hidden ) {
                b->visible = false;
            }
        }

        if(!b->visible) {
            continue;
        }

        if( !isTileOnVisibleEdgeOfSegment(segment, b) 
            && (b->tileType!=tiletype::OpenSpace
            || b->designation.bits.flow_size
            || (b->occ.bits.unit && b->creature)
            || b->building.type != BUILDINGTYPE_NA
            || b->tileeffect.type != (df::flow_type) INVALID_INDEX)) {

            //hide any tiles that are totally surrounded
            enclosedTile(segment, b);

            if(!b->visible) {
                continue;
            }

            //next see if the tile is behind something
            if(ssConfig.occlusion) {
                occlude_tile(b);
            }
        }
    }
}

void beautifySegment(WorldSegment * segment)
{
    if(!segment) {
        return;
    }

    clock_t starttime = clock();

    optimizeSegment(segment);
    addSegmentExtras(segment);

    segment->processed = 1;
    ssTimers.beautify_time = (clock() - starttime)*0.1 + ssTimers.beautify_time*0.9;
}
