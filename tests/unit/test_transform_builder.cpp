#include <gtest/gtest.h>
#include "core/coordinate/transform_builder.h"
#include "core/coordinate/coordinate_system.h"
#include "../utils/test_helpers.h"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

using namespace Tiles::Core::Geo;
using namespace Tiles::Test;

class TransformBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TransformBuilderTest, BuildYUpToZUpTransform) {
    auto transform = TransformBuilder::buildYUpToZUpTransform();
    
    // Test that Y-up (0,1,0) becomes Z-up (0,0,1)
    glm::dvec4 yUp(0.0, 1.0, 0.0, 0.0);
    glm::dvec4 result = transform * yUp;
    
    EXPECT_NEAR(result.x, 0.0, 1e-10);
    EXPECT_NEAR(result.y, 0.0, 1e-10);
    EXPECT_NEAR(result.z, 1.0, 1e-10);
    
    // Test that Z-up (0,0,1) becomes -Y (0,-1,0)
    glm::dvec4 zUp(0.0, 0.0, 1.0, 0.0);
    result = transform * zUp;
    
    EXPECT_NEAR(result.x, 0.0, 1e-10);
    EXPECT_NEAR(result.y, -1.0, 1e-10);
    EXPECT_NEAR(result.z, 0.0, 1e-10);
}

TEST_F(TransformBuilderTest, BuildZUpToYUpTransform) {
    auto yToZ = TransformBuilder::buildYUpToZUpTransform();
    auto zToY = TransformBuilder::buildZUpToYUpTransform();
    
    // They should be inverses
    glm::dmat4 identity = yToZ * zToY;
    EXPECT_TRUE(TransformBuilder::isIdentity(identity, 1e-10));
}

TEST_F(TransformBuilderTest, BuildCenterOffsetMatrix) {
    glm::dvec3 center(100.0, 200.0, 50.0);
    auto transform = TransformBuilder::buildCenterOffsetMatrix(center);
    
    // Test that center point becomes origin
    glm::dvec4 point(center, 1.0);
    glm::dvec4 result = transform * point;
    
    EXPECT_NEAR(result.x, 0.0, 1e-10);
    EXPECT_NEAR(result.y, 0.0, 1e-10);
    EXPECT_NEAR(result.z, 0.0, 1e-10);
}

TEST_F(TransformBuilderTest, BuildUnitScaleMatrix) {
    // Scale from millimeters to meters
    auto transform = TransformBuilder::buildUnitScaleMatrix(0.001, 1.0);
    
    glm::dvec4 point(1000.0, 2000.0, 500.0, 1.0);  // 1000mm = 1m
    glm::dvec4 result = transform * point;
    
    EXPECT_NEAR(result.x, 1.0, 1e-10);
    EXPECT_NEAR(result.y, 2.0, 1e-10);
    EXPECT_NEAR(result.z, 0.5, 1e-10);
}

TEST_F(TransformBuilderTest, ValidateTransform_Valid) {
    glm::dmat4 valid(1.0);
    EXPECT_TRUE(TransformBuilder::validateTransform(valid));
    
    auto enuTransform = TransformBuilder::buildEnuToEcefTransform(116.0, 40.0, 0.0);
    EXPECT_TRUE(TransformBuilder::validateTransform(enuTransform));
}

TEST_F(TransformBuilderTest, ValidateTransform_Invalid) {
    // Matrix with NaN
    glm::dmat4 invalid;
    invalid[0][0] = std::nan("");
    EXPECT_FALSE(TransformBuilder::validateTransform(invalid));
}

TEST_F(TransformBuilderTest, IsIdentity) {
    glm::dmat4 identity(1.0);
    EXPECT_TRUE(TransformBuilder::isIdentity(identity, 1e-10));
    
    glm::dmat4 notIdentity = glm::translate(glm::dmat4(1.0), glm::dvec3(1.0, 0.0, 0.0));
    EXPECT_FALSE(TransformBuilder::isIdentity(notIdentity, 1e-10));
}

TEST_F(TransformBuilderTest, SerializeDeserializeMatrix) {
    glm::dmat4 original = TransformBuilder::buildEnuToEcefTransform(116.0, 40.0, 100.0);
    
    auto serialized = TransformBuilder::serializeMatrix(original);
    EXPECT_EQ(serialized.size(), 16);
    
    auto deserialized = TransformBuilder::deserializeMatrix(serialized);
    
    EXPECT_TRUE(mat4Near(original, deserialized, 1e-10));
}

TEST_F(TransformBuilderTest, BuildEnuToEcefTransform) {
    double lon = 116.0;
    double lat = 40.0;
    double height = 100.0;
    
    auto transform = TransformBuilder::buildEnuToEcefTransform(lon, lat, height);
    
    // Verify it's a valid transform
    EXPECT_TRUE(TransformBuilder::validateTransform(transform));
    
    // Verify ENU origin maps to correct ECEF
    glm::dvec4 enuOrigin(0.0, 0.0, 0.0, 1.0);
    glm::dvec4 ecef = transform * enuOrigin;
    
    // The ECEF coordinates should be reasonable for the location
    double distance = glm::length(glm::dvec3(ecef));
    EXPECT_GT(distance, 6300000.0);  // > Earth radius
    EXPECT_LT(distance, 6400000.0);  // < Earth radius + 10km
}

TEST_F(TransformBuilderTest, BuildTilesetTransform_Geographic) {
    CoordinateSystem cs;
    cs.type = CoordinateType::GEOGRAPHIC;
    cs.epsgCode = "EPSG:4326";
    cs.upAxis = UpAxis::Z_UP;
    
    glm::dvec3 center(116.0, 40.0, 100.0);
    
    auto transform = TransformBuilder::buildTilesetTransform(cs, center);
    
    EXPECT_TRUE(TransformBuilder::validateTransform(transform));
}

TEST_F(TransformBuilderTest, BuildTilesetTransform_Projected) {
    CoordinateSystem cs;
    cs.type = CoordinateType::PROJECTED;
    cs.epsgCode = "EPSG:4548";
    cs.upAxis = UpAxis::Z_UP;
    
    glm::dvec3 center(388231.963, 3168121.924, 0.0);
    
    auto transform = TransformBuilder::buildTilesetTransform(cs, center);
    
    EXPECT_TRUE(TransformBuilder::validateTransform(transform));
}
