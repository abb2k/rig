#include "Types.hpp"

#include <cmath>

// ===================
// Vec2

Vec2 Vec2::operator/(float s) const { return { x / s, y / s }; }
Vec2& Vec2::operator/=(float s) { x /= s; y /= s; return *this; }
Vec2 Vec2::operator*(float s) const { return { x * s, y * s }; }
Vec2& Vec2::operator*=(float s) { x *= s; y *= s; return *this; }

// ===================
// Vec3

Vec3 Vec3::operator/(float s) const { return { x / s, y / s, z / s }; }
Vec3& Vec3::operator/=(float s) { x /= s; y /= s; z /= s; return *this; }
Vec3 Vec3::operator*(float s) const { return { x * s, y * s, z * s }; }
Vec3& Vec3::operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

Vec3 Vec3::lerp(const Vec3& a, const Vec3& b, float t) {
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
}

// ===================
// Vec4

Vec4 Vec4::operator/(float s) const { return { x / s, y / s, z / s, w / s }; }
Vec4& Vec4::operator/=(float s) { x /= s; y /= s; z /= s; w /= s; return *this; }
Vec4 Vec4::operator*(float s) const { return { x * s, y * s, z * s, w * s }; }
Vec4& Vec4::operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }

// ===================
// Quat

Quat Quat::operator/(float s) const { return { x / s, y / s, z / s, w / s }; }
Quat& Quat::operator/=(float s) { x /= s; y /= s; z /= s; w /= s; return *this; }
Quat Quat::operator*(float s) const { return { x * s, y * s, z * s, w * s }; }
Quat& Quat::operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }

float Quat::magnetude(){
    return sqrt(
        x * x +
        y * y +
        z * z +
        w * w
    );
}

Vec3 Quat::toEuler() const {
    float roll = std::atan2(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y));

    float sinp = 2.0f * (w * y - z * x);
    float pitch;
    if (std::abs(sinp) >= 1.0f)
        pitch = std::copysign(3.1415926535f / 2.0f, sinp);
    else
        pitch = std::asin(sinp);

    float yaw = std::atan2(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z));

    return { roll, pitch, yaw };
}

Quat Quat::nlerp(const Quat& a, const Quat& b, float t) {
    float dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    float sign = (dot < 0.0f) ? -1.0f : 1.0f;

    Quat result = {
        a.x + (b.x * sign - a.x) * t,
        a.y + (b.y * sign - a.y) * t,
        a.z + (b.z * sign - a.z) * t,
        a.w + (b.w * sign - a.w) * t
    };

    float mag = result.magnetude();
    if (mag > 0.0001f) {
        return {result.x/mag, result.y/mag, result.z/mag, result.w/mag};
    }
    return Quat::identity();
}

// ===================
// Mat4

Mat4::Mat4() { identity(); }
void Mat4::identity() {
    for(int i=0; i<16; ++i) m[i] = (i%5 == 0) ? 1.0f : 0.0f;
}
Mat4 Mat4::operator*(const Mat4& b) const {
    Mat4 res;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            res.m[c*4 + r] = 
                m[0*4 + r] * b.m[c*4 + 0] +
                m[1*4 + r] * b.m[c*4 + 1] +
                m[2*4 + r] * b.m[c*4 + 2] +
                m[3*4 + r] * b.m[c*4 + 3];
        }
    }
    return res;
}
Vec3 Mat4::operator*(const Vec3& v) const {
    return {
        m[0]*v.x + m[4]*v.y + m[8]*v.z + m[12],
        m[1]*v.x + m[5]*v.y + m[9]*v.z + m[13],
        m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]
    };
}
Mat4 Mat4::fromTRS(const Vec3& t, const Quat& q, const Vec3& s) {
    Mat4 res;
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    res.m[0] = (1.0f - 2.0f * (yy + zz)) * s.x;
    res.m[1] = (2.0f * (xy + wz)) * s.x;
    res.m[2] = (2.0f * (xz - wy)) * s.x;
    res.m[3] = 0.0f;

    res.m[4] = (2.0f * (xy - wz)) * s.y;
    res.m[5] = (1.0f - 2.0f * (xx + zz)) * s.y;
    res.m[6] = (2.0f * (yz + wx)) * s.y;
    res.m[7] = 0.0f;

    res.m[8] = (2.0f * (xz + wy)) * s.z;
    res.m[9] = (2.0f * (yz - wx)) * s.z;
    res.m[10] = (1.0f - 2.0f * (xx + yy)) * s.z;
    res.m[11] = 0.0f;

    res.m[12] = t.x; res.m[13] = t.y; res.m[14] = t.z; res.m[15] = 1.0f;
    return res;
}