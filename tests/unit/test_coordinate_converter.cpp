#include <gtest/gtest.h>
#include "core/coordinate/coordinate_converter.h"
#include "core/coordinate/coordinate_system.h"
#include "../utils/test_helpers.h"
#include <cmath>

using namespace Tiles::Core::Geo;
using namespace Tiles::Test;

class CoordinateConverterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test geographic to ECEF conversion with known points
TEST_F(CoordinateConverterTest, GeographicToEcef_KnownPoints) {
    // Beijing Tiananmen (116.3974°E, 39.9093°N, 0m)
    auto ecef = CoordinateConverter::geographicToEcef(116.3974, 39.9093, 0.0);

    // Verify the result is reasonable (within expected range for Beijing)
    // Beijing ECEF coordinates should be approximately:
    // X: -2,100,000 to -2,200,000 m
    // Y: 4,380,000 to 4,400,000 m
    // Z: 4,050,000 to 4,100,000 m
    EXPECT_GT(ecef.x, -2200000.0);
    EXPECT_LT(ecef.x, -2100000.0);
    EXPECT_GT(ecef.y, 4380000.0);
    EXPECT_LT(ecef.y, 4400000.0);
    EXPECT_GT(ecef.z, 4050000.0);
    EXPECT_LT(ecef.z, 4100000.0);
}

TEST_F(CoordinateConverterTest, GeographicToEcef_EquatorPrimeMeridian) {
    // Origin (0°, 0°, 0m) - should be on X axis at equator radius
    auto ecef = CoordinateConverter::geographicToEcef(0.0, 0.0, 0.0);

    EXPECT_NEAR(ecef.x, 6378137.0, 0.1);  // WGS84 semi-major axis
    EXPECT_NEAR(ecef.y, 0.0, 0.1);
    EXPECT_NEAR(ecef.z, 0.0, 0.1);
}

TEST_F(CoordinateConverterTest, GeographicToEcef_NorthPole) {
    // North Pole (0°, 90°, 0m)
    auto ecef = CoordinateConverter::geographicToEcef(0.0, 90.0, 0.0);

    EXPECT_NEAR(ecef.x, 0.0, 0.1);
    EXPECT_NEAR(ecef.y, 0.0, 0.1);
    EXPECT_NEAR(ecef.z, 6356752.3, 0.1);  // WGS84 semi-minor axis
}

// Test round-trip conversion
TEST_F(CoordinateConverterTest, RoundTripConversion) {
    double origLon = 116.3974;
    double origLat = 39.9093;
    double origHeight = 100.0;

    // Geographic -> ECEF
    auto ecef = CoordinateConverter::geographicToEcef(origLon, origLat, origHeight);

    // ECEF -> Geographic
    double outLon, outLat, outHeight;
    CoordinateConverter::ecefToGeographic(ecef, outLon, outLat, outHeight);

    // Verify precision
    EXPECT_NEAR(origLon, outLon, 1e-6);
    EXPECT_NEAR(origLat, outLat, 1e-6);
    EXPECT_NEAR(origHeight, outHeight, 1e-3);
}

// Test ENU matrix calculation
TEST_F(CoordinateConverterTest, EnuToEcefMatrix_Orthogonality) {
    auto matrix = CoordinateConverter::calcEnuToEcefMatrix(116.0, 40.0, 100.0);

    // Check orthogonality
    EXPECT_TRUE(isOrthogonal(matrix, 1e-10));

    // Extract basis vectors
    glm::dvec3 east(matrix[0]);
    glm::dvec3 north(matrix[1]);
    glm::dvec3 up(matrix[2]);

    // Check orthonormality
    EXPECT_TRUE(isOrthonormal(east, north, up, 1e-10));
}

TEST_F(CoordinateConverterTest, EnuToEcefMatrix_Translation) {
    double lon = 116.0;
    double lat = 40.0;
    double height = 100.0;

    auto matrix = CoordinateConverter::calcEnuToEcefMatrix(lon, lat, height);

    // The translation part should be the ECEF coordinates of the origin
    auto expectedEcef = CoordinateConverter::geographicToEcef(lon, lat, height);

    EXPECT_NEAR(matrix[3][0], expectedEcef.x, 0.001);
    EXPECT_NEAR(matrix[3][1], expectedEcef.y, 0.001);
    EXPECT_NEAR(matrix[3][2], expectedEcef.z, 0.001);
}

// Test projected to geographic conversion
TEST_F(CoordinateConverterTest, ProjectedToGeographic_EPSG4548) {
    // CGCS2000 3-degree Gauss-Kruger zone 39 (117°E central meridian)
    // Test point: approximate coordinates for Nanchang area
    glm::dvec3 projected(388231.963, 3168121.924, 0.0);

    auto geo = CoordinateConverter::projectedToGeographic(projected, "EPSG:4548");

    // Verify the result is in reasonable range for the area
    EXPECT_GT(geo.x, 115.0);  // Longitude > 115°E
    EXPECT_LT(geo.x, 117.0);  // Longitude < 117°E
    EXPECT_GT(geo.y, 28.0);   // Latitude > 28°N
    EXPECT_LT(geo.y, 30.0);   // Latitude < 30°N
}

// Test geographic to local meter conversion
TEST_F(CoordinateConverterTest, GeographicToLocalMeter) {
    // Center point
    glm::dvec3 center(116.0, 40.0, 0.0);

    // Point 0.01 degrees east
    glm::dvec3 east(116.01, 40.0, 0.0);
    auto localEast = CoordinateConverter::geographicToLocalMeter(east, center);

    // Should be approximately 1km east (X positive)
    EXPECT_GT(localEast.x, 800.0);   // > 800m
    EXPECT_LT(localEast.x, 1200.0);  // < 1200m
    EXPECT_NEAR(localEast.y, 0.0, 10.0);  // ~0m north

    // Point 0.01 degrees north
    glm::dvec3 north(116.0, 40.01, 0.0);
    auto localNorth = CoordinateConverter::geographicToLocalMeter(north, center);

    // Should be approximately 1.1km north (Y positive)
    EXPECT_NEAR(localNorth.x, 0.0, 10.0);   // ~0m east
    EXPECT_GT(localNorth.y, 1000.0);        // > 1000m
    EXPECT_LT(localNorth.y, 1200.0);        // < 1200m
}

// Test ECEF to geographic conversion
TEST_F(CoordinateConverterTest, EcefToGeographic_KnownPoints) {
    // Test round-trip conversion for accuracy
    double origLon = 116.3974;
    double origLat = 39.9093;
    double origHeight = 0.0;

    // Convert to ECEF
    auto ecef = CoordinateConverter::geographicToEcef(origLon, origLat, origHeight);

    // Convert back to geographic
    double lon, lat, height;
    CoordinateConverter::ecefToGeographic(ecef, lon, lat, height);

    // Verify round-trip accuracy
    EXPECT_NEAR(lon, origLon, 1e-6);  // ~0.1mm precision
    EXPECT_NEAR(lat, origLat, 1e-6);
    EXPECT_NEAR(height, origHeight, 1e-3);  // 1mm precision
}

// Test constants
TEST_F(CoordinateConverterTest, WGS84Constants) {
    EXPECT_DOUBLE_EQ(CoordinateConverter::WGS84_A, 6378137.0);
    EXPECT_DOUBLE_EQ(CoordinateConverter::WGS84_F, 1.0 / 298.257223563);
    EXPECT_NEAR(CoordinateConverter::PI, 3.14159265358979323846, 1e-15);
}
