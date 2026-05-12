#pragma once

#include <Geode/platform/platform.hpp>

using namespace geode::prelude;

#ifdef GEODE_IS_WINDOWS
    #if defined(RIG_EXPORTING) || defined(rig_EXPORTS)
        #define RIG_DLL __declspec(dllexport)
    #else
        #define RIG_DLL __declspec(dllimport)
    #endif
#else
    #define RIG_DLL __attribute__((visibility("default")))
#endif

#define GLB_PIXEL_SCALE 100.0f

#define GLB_ATTRIBUTE_POSITION "POSITION"
#define GLB_ATTRIBUTE_UV "TEXCOORD_0"
#define GLB_ATTRIBUTE_WEIGHTS "WEIGHTS_0"
#define GLB_ATTRIBUTE_JOINTS "JOINTS_0"

#define GLB_ANIM_PATH_TRANSLATION "translation"
#define GLB_ANIM_PATH_SCALE "scale"
#define GLB_ANIM_PATH_ROTATION "rotation"

#define GLB_ANIM_INTERPOLATION_LINEAR "LINEAR"
#define GLB_ANIM_INTERPOLATION_CUBICSPLINE "CUBICSPLINE"
#define GLB_ANIM_INTERPOLATION_STEP "STEP"
#define GLB_ANIM_INTERPOLATION_CUBIC "CUBIC"
#define GLB_ANIM_INTERPOLATION_HERMITE "HERMITE"
#define GLB_ANIM_INTERPOLATION_BEZIER "BEZIER"

struct Vec2 {
    float x;
    float y;

    Vec2 operator/(float s) const;
    Vec2& operator/=(float s);

    Vec2 operator*(float s) const;
    Vec2& operator*=(float s);
};

struct Vec3 {
    float x;
    float y;
    float z;

    static Vec3 zero() { return {0,0,0}; }
    static Vec3 one() { return {1,1,1}; }

    Vec3 operator/(float s) const;
    Vec3& operator/=(float s);

    Vec3 operator*(float s) const;
    Vec3& operator*=(float s);

    static Vec3 lerp(const Vec3& a, const Vec3& b, float t);
};

struct Vec4 {
    float x;
    float y;
    float z;
    float w;

    Vec4 operator/(float s) const;
    Vec4& operator/=(float s);

    Vec4 operator*(float s) const;
    Vec4& operator*=(float s);
};

struct Quat { 
    float x;
    float y;
    float z;
    float w;

    static Quat identity() { return {0,0,0,1}; }

    Quat operator/(float s) const;
    Quat& operator/=(float s);

    Quat operator*(float s) const;
    Quat& operator*=(float s);

    float magnetude();

    Vec3 toEuler() const;

    static Quat nlerp(const Quat& a, const Quat& b, float t);
};

struct Mat4 {
    float m[16];

    Mat4();

    void identity();

    Mat4 operator*(const Mat4& other) const;

    Vec3 operator*(const Vec3& v) const;

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