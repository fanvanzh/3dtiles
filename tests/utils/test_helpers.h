#pragma once

#include <glm/glm.hpp>
#include <cmath>

namespace Tiles::Test {

/**
 * Compare two vectors with tolerance
 */
inline bool vec3Near(const glm::dvec3& a, const glm::dvec3& b, double tolerance = 1e-6) {
    return std::abs(a.x - b.x) < tolerance &&
           std::abs(a.y - b.y) < tolerance &&
           std::abs(a.z - b.z) < tolerance;
}

/**
 * Compare two matrices with tolerance
 */
inline bool mat4Near(const glm::dmat4& a, const glm::dmat4& b, double tolerance = 1e-9) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (std::abs(a[i][j] - b[i][j]) > tolerance) {
                return false;
            }
        }
    }
    return true;
}

/**
 * Check if matrix is orthogonal (R^T * R = I)
 */
inline bool isOrthogonal(const glm::dmat4& m, double tolerance = 1e-10) {
    glm::dmat3 r(m);
    glm::dmat3 identity = glm::transpose(r) * r;
    
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double expected = (i == j) ? 1.0 : 0.0;
            if (std::abs(identity[i][j] - expected) > tolerance) {
                return false;
            }
        }
    }
    return true;
}

/**
 * Check if basis vectors are orthonormal
 */
inline bool isOrthonormal(const glm::dvec3& x, const glm::dvec3& y, const glm::dvec3& z, 
                          double tolerance = 1e-10) {
    // Check orthogonality
    if (std::abs(glm::dot(x, y)) > tolerance) return false;
    if (std::abs(glm::dot(x, z)) > tolerance) return false;
    if (std::abs(glm::dot(y, z)) > tolerance) return false;
    
    // Check unit length
    if (std::abs(glm::length(x) - 1.0) > tolerance) return false;
    if (std::abs(glm::length(y) - 1.0) > tolerance) return false;
    if (std::abs(glm::length(z) - 1.0) > tolerance) return false;
    
    return true;
}

}
