#pragma once
#include "Math/BoundingBox.h"

class OctreeNode
{
public:
    int minX;
    int maxX;
    int minY;
    int maxY;
    int minZ;
    int maxZ;
    Math::AxisAlignedBox box;
    bool isLeafNode = false;
    OctreeNode* leftBottomFront;
    OctreeNode* rightBottomFront;
    OctreeNode* leftTopFront;
    OctreeNode* rightTopFront;
    OctreeNode* leftBottomBack;
    OctreeNode* rightBottomBack;
    OctreeNode* leftTopBack;
    OctreeNode* rightTopBack;

    OctreeNode(int minX, int maxX, int minY, int maxY, int minZ, int maxZ, bool isLeafNode)
        : minX(minX), maxX(maxX), minY(minY), maxY(maxY), minZ(minZ), maxZ(maxZ), isLeafNode(isLeafNode)
    {}
    
};
