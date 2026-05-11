#pragma once

using namespace geode::prelude;

#define GLB_PIXEL_SCALE 100.0f

struct Vec2 {
    float x;
    float y;
};

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Vec4 {
    float x;
    float y;
    float z;
    float w;
};

struct Quat { 
    float x;
    float y;
    float z;
    float w;
};

struct Mat4 {
    float m[16];

    Mat4();

    void identity();

    static Mat4 multiply(const Mat4& a, const Mat4& b);

    Vec3 multiplyVec3(const Vec3& v) const;

    static Mat4 fromTRS(const Vec3& t, const Quat& q, const Vec3& s);
};

struct SkinnedVertex {
    Vec3 pos;
    Vec2 uv;
    ccColor4F color;
};

struct SubMesh {
    std::string name;
    std::string nodeName;
    int nodeIndex = -1;
    std::vector<Vec3> basePositions;
    std::vector<Vec2> uvs;
    std::vector<Vec4> weights;
    std::vector<Vec4> joints;
    std::vector<unsigned short> indices;
    std::vector<SkinnedVertex> renderVertices;
    int embeddedImageIndex = -1;
};

struct Track {
    std::vector<float> times;
    std::vector<std::vector<float>> values;
    int targetNode;
    std::string path;
    std::string interpolation;
};