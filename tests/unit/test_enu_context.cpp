#include <gtest/gtest.h>
#include "core/coordinate/enu_context.h"
#include "core/coordinate/coordinate_converter.h"
#include "../utils/test_helpers.h"
#include <cmath>

using namespace Tiles::Core::Geo;
using namespace Tiles::Test;

class ENUContextTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ENUContextTest, FromGeographic) {
    double lon = 116.0;
    double lat = 40.0;
    double height = 50.0;
    
    auto context = ENUContext::fromGeographic(lon, lat, height);
    
    // Verify origin coordinates
    EXPECT_DOUBLE_EQ(context.originLon, lon);
    EXPECT_DOUBLE_EQ(context.originLat, lat);
    EXPECT_DOUBLE_EQ(context.originHeight, height);
    
    // Verify ECEF origin
    auto expectedEcef = CoordinateConverter::geographicToEcef(lon, lat, height);
    EXPECT_NEAR(context.originEcef.x, expectedEcef.x, 0.001);
    EXPECT_NEAR(context.originEcef.y, expectedEcef.y, 0.001);
    EXPECT_NEAR(context.originEcef.z, expectedEcef.z, 0.001);
}

TEST_F(ENUContextTest, FromEcef) {
    // Beijing ECEF coordinates
    glm::dvec3 ecefOrigin(-2148581.0, 4393316.0, 4077985.0);
    
    auto context = ENUContext::fromEcef(ecefOrigin);
    
    // Verify ECEF origin
    EXPECT_NEAR(context.originEcef.x, ecefOrigin.x, 0.001);
    EXPECT_NEAR(context.originEcef.y, ecefOrigin.y, 0.001);
    EXPECT_NEAR(context.originEcef.z, ecefOrigin.z, 0.001);
    
    // Verify geographic coordinates are reasonable (Beijing area)
    EXPECT_GT(context.originLon, 115.0);
    EXPECT_LT(context.originLon, 118.0);
    EXPECT_GT(context.originLat, 38.0);
    EXPECT_LT(context.originLat, 41.0);
}

TEST_F(ENUContextTest, MatrixInverse) {
    double lon = 116.0;
    double lat = 40.0;
    double height = 0.0;
    
    auto context = ENUContext::fromGeographic(lon, lat, height);
    
    // Verify that matrices are inverses
    glm::dmat4 identity = context.ecefToEnuMatrix * context.enuToEcefMatrix;
    
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            double expected = (i == j) ? 1.0 : 0.0;
            EXPECT_NEAR(identity[i][j], expected, 1e-9);  // 放宽精度要求
        }
    }
}

TEST_F(ENUContextTest, Validate_ValidContext) {
    auto context = ENUContext::fromGeographic(116.0, 40.0, 0.0);
    EXPECT_TRUE(context.validate());
}

TEST_F(ENUContextTest, Validate_InvalidContext) {
    // Create an invalid context with NaN values
    ENUContext invalidContext;
    invalidContext.enuToEcefMatrix[0][0] = std::nan("");
    EXPECT_FALSE(invalidContext.validate());
}

TEST_F(ENUContextTest, EnuToEcef_Transform) {
    double lon = 116.0;
    double lat = 40.0;
    double height = 0.0;
    
    auto context = ENUContext::fromGeographic(lon, lat, height);
    
    // Transform ENU origin (0,0,0) should give ECEF origin
    glm::dvec3 enuOrigin(0.0, 0.0, 0.0);
    auto ecef = context.enuToEcef(enuOrigin);
    
    EXPECT_NEAR(ecef.x, context.originEcef.x, 0.001);
    EXPECT_NEAR(ecef.y, context.originEcef.y, 0.001);
    EXPECT_NEAR(ecef.z, context.originEcef.z, 0.001);
}

TEST_F(ENUContextTest, EcefToEnu_Transform) {
    double lon = 116.0;
    double lat = 40.0;
    double height = 0.0;
    
    auto context = ENUContext::fromGeographic(lon, lat, height);
    
    // Transform ECEF origin should give ENU origin (0,0,0)
    auto enu = context.ecefToEnu(context.originEcef);
    
    EXPECT_NEAR(enu.x, 0.0, 0.001);
    EXPECT_NEAR(enu.y, 0.0, 0.001);
    EXPECT_NEAR(enu.z, 0.0, 0.001);
}

TEST_F(ENUContextTest, RoundTripTransform) {
    double lon = 116.0;
    double lat = 40.0;
    double height = 100.0;
    
    auto context = ENUContext::fromGeographic(lon, lat, height);
    
    // Test point 1000m east, 2000m north, 50m up in ENU
    glm::dvec3 enuPoint(1000.0, 2000.0, 50.0);
    
    // ENU -> ECEF -> ENU
    auto ecef = context.enuToEcef(enuPoint);
    auto enuBack = context.ecefToEnu(ecef);
    
    EXPECT_NEAR(enuBack.x, enuPoint.x, 0.001);
    EXPECT_NEAR(enuBack.y, enuPoint.y, 0.001);
    EXPECT_NEAR(enuBack.z, enuPoint.z, 0.001);
}
