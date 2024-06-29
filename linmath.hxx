#ifndef LINMATH_H
#define LINMATH_H

#include <assert.h>
#include <cmath>
#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>
#include <iostream>

inline float deg_2_rad(float deg) { return deg * (M_PI / 180.0f); }
inline float rad_2_deg(float rad) { return rad * (180.0f / M_PI); }

class Vector3 {
public:
  inline Vector3() : _x(0.0f), _y(0.0f), _z(0.0f) { }
  inline Vector3(float fill) : _x(fill), _y(fill), _z(fill) { }
  inline Vector3(float x, float y, float z) : _x(x), _y(y), _z(z) { }

  inline static Vector3 right() { return Vector3(1.0f, 0.0f, 0.0f); }
  inline static Vector3 left() { return Vector3(-1.0f, 0.0f, 0.0f); }
  inline static Vector3 up() { return Vector3(0.0f, 0.0f, 1.0f); }
  inline static Vector3 down() { return Vector3(0.0f, 0.0f, -1.0f); }
  inline static Vector3 forward() { return Vector3(0.0f, 1.0f, 0.0f); }
  inline static Vector3 back() { return Vector3(0.0f, -1.0f, 0.0f); }

  inline const float *get_data() const { return _data; }
  inline float get_x() { return _data[0]; }
  inline float get_y() { return _data[1]; }
  inline float get_z() { return _data[2]; }
  inline float &operator[] (int i) { assert(i >= 0 && i <= 2); return _data[i]; }
  inline const float &operator[] (int i) const { assert(i >= 0 && i <= 2); return _data[i]; }

  inline Vector3 &operator *= (float scalar) {
    _x *= scalar;
    _y *= scalar;
    _z *= scalar;
    return *this;
  }
  inline Vector3 operator * (float scalar) const {
    Vector3 out(*this);
    out *= scalar;
    return out;
  }
  inline Vector3 &operator /= (float scalar) {
    _x /= scalar;
    _y /= scalar;
    _z /= scalar;
    return *this;
  }
  inline Vector3 operator /= (float scalar) const {
    Vector3 out(*this);
    out /= scalar;
    return out;
  }
  inline Vector3 &operator -= (const Vector3 &other) {
    _x -= other._x;
    _y -= other._y;
    _z -= other._z;
    return *this;
  }
  inline Vector3 operator - (const Vector3 &other) {
    Vector3 out(*this);
    out -= other;
    return out;
  }
  inline Vector3 &operator += (const Vector3 &other) {
    _x += other._x;
    _y += other._y;
    _z += other._z;
    return *this;
  }
  inline Vector3 operator + (const Vector3 &other) {
    Vector3 out(*this);
    out += other;
    return out;
  }

  inline float dot(const Vector3 &other) const {
    return _x * other._x + _y * other._y + _z * other._z;
  }

  inline Vector3 cross(const Vector3 &other) const {
    Vector3 out;
    out._x = _y * other._z - _z * other._y;
    out._y = -(_x * other._z - _z * other._x);
    out._z = _x * other._y - _y * other._x;
    return out;
  }

  inline float length_squared() const { return dot(*this); }
  inline float length() const { return sqrt(length_squared()); }

  inline bool normalize() {
    float len = length();
    if (len > FLT_EPSILON) {
      _x /= len;
      _y /= len;
      _z /= len;
      return true;
    } else {
      return false;
    }
  }

  inline Vector3 normalized() const {
    Vector3 copy(*this);
    copy.normalize();
    return copy;
  }

private:
  union {
    float _data[3];
    struct {
      float _x, _y, _z;
    };
  };
};

class Matrix4x4 {
public:
  Matrix4x4() = default;
  Matrix4x4(float fill) {
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        _data[i][j] = fill;
      }
    }
  }

  inline static Matrix4x4 identity() {
    Matrix4x4 out(0.0f);
    out._data[0][0] = 1.0f;
    out._data[1][1] = 1.0f;
    out._data[2][2] = 1.0f;
    out._data[3][3] = 1.0f;
    return out;
  }

  inline void set_cell(int row, int col, float val) { _data[row][col] = val; }
  inline float get_cell(int row, int col) const { return _data[row][col]; }

  inline void set_row(int row, float x, float y, float z, float w) {
    _data[row][0] = x;
    _data[row][1] = y;
    _data[row][2] = z;
    _data[row][3] = w;
  }
  inline const float *get_row(int row) const {
    return _data[row];
  }

  inline const float *get_data() const { return (const float *)_data; }

  inline Matrix4x4 transposed() const {
    Matrix4x4 out;
    // Flip rows and columns.
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        out._data[i][j] = _data[j][i];
      }
    }
    return out;
  }
  inline Matrix4x4 &transpose() {
    *this = transposed();
    return *this;
  }

  inline Matrix4x4 operator * (const Matrix4x4 &other) const {
    Matrix4x4 out;
    // 1st row.
    out._data[0][0] = _data[0][0] * other._data[0][0] + _data[0][1] * other._data[1][0] + _data[0][2] * other._data[2][0] + _data[0][3] * other._data[3][0];
    out._data[0][1] = _data[0][0] * other._data[0][1] + _data[0][1] * other._data[1][1] + _data[0][2] * other._data[2][1] + _data[0][3] * other._data[3][1];
    out._data[0][2] = _data[0][0] * other._data[0][2] + _data[0][1] * other._data[1][2] + _data[0][2] * other._data[2][2] + _data[0][3] * other._data[3][2];
    out._data[0][3] = _data[0][0] * other._data[0][3] + _data[0][1] * other._data[1][3] + _data[0][2] * other._data[2][3] + _data[0][3] * other._data[3][3];
    // 2nd row.
    out._data[1][0] = _data[1][0] * other._data[0][0] + _data[1][1] * other._data[1][0] + _data[1][2] * other._data[2][0] + _data[1][3] * other._data[3][0];
    out._data[1][1] = _data[1][0] * other._data[0][1] + _data[1][1] * other._data[1][1] + _data[1][2] * other._data[2][1] + _data[1][3] * other._data[3][1];
    out._data[1][2] = _data[1][0] * other._data[0][2] + _data[1][1] * other._data[1][2] + _data[1][2] * other._data[2][2] + _data[1][3] * other._data[3][2];
    out._data[1][3] = _data[1][0] * other._data[0][3] + _data[1][1] * other._data[1][3] + _data[1][2] * other._data[2][3] + _data[1][3] * other._data[3][3];
    // 3rd row.
    out._data[2][0] = _data[2][0] * other._data[0][0] + _data[2][1] * other._data[1][0] + _data[2][2] * other._data[2][0] + _data[2][3] * other._data[3][0];
    out._data[2][1] = _data[2][0] * other._data[0][1] + _data[2][1] * other._data[1][1] + _data[2][2] * other._data[2][1] + _data[2][3] * other._data[3][1];
    out._data[2][2] = _data[2][0] * other._data[0][2] + _data[2][1] * other._data[1][2] + _data[2][2] * other._data[2][2] + _data[2][3] * other._data[3][2];
    out._data[2][3] = _data[2][0] * other._data[0][3] + _data[2][1] * other._data[1][3] + _data[2][2] * other._data[2][3] + _data[2][3] * other._data[3][3];
    // 4th row.
    out._data[3][0] = _data[3][0] * other._data[0][0] + _data[3][1] * other._data[1][0] + _data[3][2] * other._data[2][0] + _data[3][3] * other._data[3][0];
    out._data[3][1] = _data[3][0] * other._data[0][1] + _data[3][1] * other._data[1][1] + _data[3][2] * other._data[2][1] + _data[3][3] * other._data[3][1];
    out._data[3][2] = _data[3][0] * other._data[0][2] + _data[3][1] * other._data[1][2] + _data[3][2] * other._data[2][2] + _data[3][3] * other._data[3][2];
    out._data[3][3] = _data[3][0] * other._data[0][3] + _data[3][1] * other._data[1][3] + _data[3][2] * other._data[2][3] + _data[3][3] * other._data[3][3];
    return out;
  }
  inline Matrix4x4 &operator *= (const Matrix4x4 &other) {
    *this = (*this) * other;
    return *this;
  }

  inline Matrix4x4 inverted() const {
    float a2323 = _data[2][2] * _data[3][3] - _data[2][3] * _data[3][2];
    float a1323 = _data[2][1] * _data[3][3] - _data[2][3] * _data[3][1];
    float a1223 = _data[2][1] * _data[3][2] - _data[2][2] * _data[3][1];
    float a0323 = _data[2][0] * _data[3][3] - _data[2][3] * _data[3][0];
    float a0223 = _data[2][0] * _data[3][2] - _data[2][2] * _data[3][0];
    float a0123 = _data[2][0] * _data[3][1] - _data[2][1] * _data[3][0];
    float a2313 = _data[1][2] * _data[3][3] - _data[1][3] * _data[3][2];
    float a1313 = _data[1][1] * _data[3][3] - _data[1][3] * _data[3][1];
    float a1213 = _data[1][1] * _data[3][2] - _data[1][2] * _data[3][1];
    float a2312 = _data[1][2] * _data[2][3] - _data[1][3] * _data[2][2];
    float a1312 = _data[1][1] * _data[2][3] - _data[1][3] * _data[2][1];
    float a1212 = _data[1][1] * _data[2][2] - _data[1][2] * _data[2][1];
    float a0313 = _data[1][0] * _data[3][3] - _data[1][3] * _data[3][0];
    float a0213 = _data[1][0] * _data[3][2] - _data[1][2] * _data[3][0];
    float a0312 = _data[1][0] * _data[2][3] - _data[1][3] * _data[2][0];
    float a0212 = _data[1][0] * _data[2][2] - _data[1][2] * _data[2][0];
    float a0113 = _data[1][0] * _data[3][1] - _data[1][1] * _data[3][0];
    float a0112 = _data[1][0] * _data[2][1] - _data[1][1] * _data[2][0];

    float det =
        _data[0][0] * (_data[1][1] * a2323 - _data[1][2] * a1323 + _data[1][3] * a1223)
      - _data[0][1] * (_data[1][0] * a2323 - _data[1][2] * a0323 + _data[1][3] * a0223)
      + _data[0][2] * (_data[1][0] * a1323 - _data[1][1] * a0323 + _data[1][3] * a0123)
      - _data[0][3] * (_data[1][0] * a1223 - _data[1][1] * a0223 + _data[1][2] * a0123);
    if (abs(det) < FLT_EPSILON) {
      return Matrix4x4();
    }
    det = 1.0f / det;

    Matrix4x4 out;
    out._data[0][0] = det * (_data[1][1] * a2323 - _data[1][2] * a1323 + _data[1][3] * a1223);
    out._data[0][1] = det * -(_data[0][1] * a2323 - _data[0][2] * a1323 + _data[0][3] * a1223);
    out._data[0][2] = det * (_data[0][1] * a2313 - _data[0][2] * a1313 + _data[0][3] * a1213);
    out._data[0][3] = det * -(_data[0][1] * a2312 - _data[0][2] * a1312 + _data[0][3] * a1212);
    out._data[1][0] = det * -(_data[1][0] * a2323 - _data[1][2] * a0323 + _data[1][3] * a0223);
    out._data[1][1] = det * (_data[0][0] * a2323 - _data[0][2] * a0323 + _data[0][3] * a0223);
    out._data[1][2] = det * -(_data[0][0] * a2313 - _data[0][2] * a0313 + _data[0][3] * a0213);
    out._data[1][3] = det * (_data[0][0] * a2312 - _data[0][2] * a0312 + _data[0][3] * a0212);
    out._data[2][0] = det * (_data[1][0] * a1323 - _data[1][2] * a0323 + _data[1][3] * a0123);
    out._data[2][1] = det * -(_data[0][0] * a1323 - _data[0][1] * a0323 + _data[0][3] * a0123);
    out._data[2][2] = det * (_data[0][0] * a1313 - _data[0][1] * a0313 + _data[0][3] * a0113);
    out._data[2][3] = det * -(_data[0][0] * a1323 - _data[0][1] * a0312 + _data[0][3] * a0112);
    out._data[3][0] = det * -(_data[1][0] * a1223 - _data[1][1] * a0223 + _data[1][2] * a0123);
    out._data[3][1] = det * (_data[0][0] * a1223 - _data[0][1] * a0223 + _data[0][2] * a0123);
    out._data[3][2] = det * -(_data[0][0] * a1213 - _data[0][1] * a0213 + _data[0][2] * a0123);
    out._data[3][3] = det * (_data[0][0] * a1212 - _data[0][1] * a0212 + _data[0][2] * a0112);

    return out;
  }

  inline Matrix4x4 &invert() {
    *this = inverted();
    return *this;
  }

  /**
   * Builds a perpsective projection matrix from the given lens parameters.
   */
  inline static Matrix4x4 make_perspective_projection(float fov, float aspect, float near_dist, float far_dist) {
    if (fov <= 0.0f || aspect == 0.0f) {
      return Matrix4x4();
    }
    float tan_fov = tanf(0.5f * fov);
    // Perspective projection
    Matrix4x4 proj(0.0f);
    proj._data[0][0] = 1.0f / (aspect * tan_fov);
    proj._data[1][2] = far_dist / (far_dist - near_dist);
    proj._data[1][3] = 1.0f;
    proj._data[2][1] = -1.0f / tan_fov;
    proj._data[3][2] = -(far_dist * near_dist) / (far_dist - near_dist);
    return proj;
  }

  /**
   * Builds an orthographic projection matrix from the given parameters.
   */
  inline static Matrix4x4 make_orthographic_projection(float left, float right, float bottom, float top, float near_dist, float far_dist) {
    Matrix4x4 out(0.0f);
    out._data[0][0] = 2.0f / (right - left);
    out._data[1][1] = 2.0f / (top - bottom);
    out._data[2][2] = -2.0f / (far_dist - near_dist);
    out._data[3][0] = -(right + left) / (right - left);
    out._data[3][1] = -(top + bottom) / (top - bottom);
    out._data[3][2] = -(far_dist + near_dist) / (far_dist - near_dist);
    out._data[3][3] = 1.0f;
    return out;
  }

  inline static Matrix4x4 scale_shear_mat(const Vector3 &scale,
                                          const Vector3 &shear) {
    Matrix4x4 out = identity();
    out._data[0][0] = scale[0];
    out._data[0][1] = shear[0] * scale[0];
    out._data[0][2] = 0.0f;
    out._data[1][0] = 0.0f;
    out._data[1][1] = scale[1];
    out._data[1][2] = 0.0f;
    out._data[2][0] = shear[1] * scale[1];
    out._data[2][1] = shear[2] * scale[2];
    out._data[2][2] = scale[2];
    return out;
  }

  inline static Matrix4x4 rotate_mat_normaxis(float angle,
                                              const Vector3 &axis) {

    float angle_rad = deg_2_rad(angle);
    float s = std::sin(angle);
    float c = std::cos(angle);
    float t = 1.0f - c;

    float t0, t1, t2, s0, s1, s2;
    t0 = t * axis[0];
    t1 = t * axis[1];
    t2 = t * axis[2];
    s0 = s * axis[0];
    s1 = s * axis[1];
    s2 = s * axis[2];

    Matrix4x4 out = identity();
    out._data[0][0] = t0 * axis[0] + c;
    out._data[0][1] = t0 * axis[1] + s2;
    out._data[0][2] = t0 * axis[2] - s1;
    out._data[1][0] = t1 * axis[0] - s2;
    out._data[1][1] = t1 * axis[1] + c;
    out._data[1][2] = t1 * axis[2] + s0;
    out._data[2][0] = t2 * axis[0] + s1;
    out._data[2][1] = t2 * axis[1] - s0;
    out._data[2][2] = t2 * axis[2] + c;

    return out;
  }

  inline static Matrix4x4 from_components(const Vector3 &scale,
                                          const Vector3 &shear,
                                          const Vector3 &hpr,
                                          const Vector3 &translate) {
    Matrix4x4 out;

    // Scale and shear.
    out = scale_shear_mat(scale, shear);

    // Rotate.
    if (hpr[2] != 0.0f) {
      out *= rotate_mat_normaxis(hpr[2], Vector3::forward());
    }
    if (hpr[1] != 0.0f) {
      out *= rotate_mat_normaxis(hpr[1], Vector3::right());
    }
    if (hpr[0] != 0.0f) {
      out *= rotate_mat_normaxis(hpr[0], Vector3::up());
    }

    // Translate
    out._data[3][0] = translate[0];
    out._data[3][1] = translate[1];
    out._data[3][2] = translate[2];

    return out;
  }

private:
  // [row][column]
  float _data[4][4];
};

inline std::ostream &operator << (std::ostream &out, const Vector3 &vec) {
  out << "(" << vec[0] << ", " << vec[1] << ", " << vec[2] << ")";
  return out;
}

inline std::ostream &operator << (std::ostream &out, const Matrix4x4 &mat) {
  const float *data = mat.get_data();
  out << "(" << data[0] << " " << data[1] << " " << data[2] << " " << data[3] << "\n"
      << " " << data[4] << " " << data[5] << " " << data[6] << " " << data[7] << "\n"
      << " " << data[8] << " " << data[9] << " " << data[10] << " " << data[11] << "\n"
      << " " << data[12] << " " << data[13] << " " << data[14] << " " << data[15] << ")";
  return out;
}

#endif // LINMATH_H
