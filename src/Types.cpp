#include "Types.hpp"

Mat4::Mat4() { identity(); }
void Mat4::identity() {
    for(int i=0; i<16; ++i) m[i] = (i%5 == 0) ? 1.0f : 0.0f;
}
Mat4 Mat4::multiply(const Mat4& a, const Mat4& b) {
    Mat4 res;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            res.m[c*4 + r] = 
                a.m[0*4 + r] * b.m[c*4 + 0] +
                a.m[1*4 + r] * b.m[c*4 + 1] +
                a.m[2*4 + r] * b.m[c*4 + 2] +
                a.m[3*4 + r] * b.m[c*4 + 3];
        }
    }
    return res;
}
Vec3 Mat4::multiplyVec3(const Vec3& v) const {
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