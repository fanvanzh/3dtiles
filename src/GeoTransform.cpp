#include "GeoTransform.h"

OGRCoordinateTransformation *GeoTransform::pOgrCT = nullptr;
double GeoTransform::OriginX = 0.0;
double GeoTransform::OriginY = 0.0;
double GeoTransform::OriginZ = 0.0;
glm::dmat4 GeoTransform::EcefToEnuMatrix = glm::dmat4(1);

glm::dmat4 GeoTransform::CalcEnuToEcefMatrix(double lnt, double lat, double height_min)
{
    const double pi = std::acos(-1);
    double ellipsod_a = 40680631590769;
    double ellipsod_b = 40680631590769;
    double ellipsod_c = 40408299984661.4;

    double radian_x = lnt * pi / 180.0;
    double radian_y = lat * pi / 180.0;
    double xn = std::cos(radian_x) * std::cos(radian_y);
    double yn = std::sin(radian_x) * std::cos(radian_y);
    double zn = std::sin(radian_y);

    double x0 = ellipsod_a * xn;
    double y0 = ellipsod_b * yn;
    double z0 = ellipsod_c * zn;
    double gamma = std::sqrt(xn * x0 + yn * y0 + zn * z0);
    double px = x0 / gamma;
    double py = y0 / gamma;
    double pz = z0 / gamma;

    double dx = xn * height_min;
    double dy = yn * height_min;
    double dz = zn * height_min;

    double east_mat[3] = { -y0,x0,0 };
    double north_mat[3] = {
        (y0 * east_mat[2] - east_mat[1] * z0),
        (z0 * east_mat[0] - east_mat[2] * x0),
        (x0 * east_mat[1] - east_mat[0] * y0)
    };
    double east_normal = std::sqrt(
        east_mat[0] * east_mat[0] +
        east_mat[1] * east_mat[1] +
        east_mat[2] * east_mat[2]
    );
    double north_normal = std::sqrt(
        north_mat[0] * north_mat[0] +
        north_mat[1] * north_mat[1] +
        north_mat[2] * north_mat[2]
    );

    glm::dmat4 matrix = {
        east_mat[0] / east_normal,
        east_mat[1] / east_normal,
        east_mat[2] / east_normal,
        0,
        north_mat[0] / north_normal,
        north_mat[1] / north_normal,
        north_mat[2] / north_normal,
        0,
        xn,
        yn,
        zn,
        0,
        px + dx,
        py + dy,
        pz + dz,
        1
    };
    return matrix;
}

glm::dvec3 GeoTransform::CartographicToEcef(double lnt, double lat, double height)
{
    static const double pi = std::acos(-1);
    double ellipsod_a = 40680631590769;
    double ellipsod_b = 40680631590769;
    double ellipsod_c = 40408299984661.4;
    lnt = lnt * pi / 180.0;
    lat = lat * pi / 180.0;

    double xn = std::cos(lnt) * std::cos(lat);
    double yn = std::sin(lnt) * std::cos(lat);
    double zn = std::sin(lat);

    double x0 = ellipsod_a * xn;
    double y0 = ellipsod_b * yn;
    double z0 = ellipsod_c * zn;
    double gamma = std::sqrt(xn * x0 + yn * y0 + zn * z0);
    double px = x0 / gamma;
    double py = y0 / gamma;
    double pz = z0 / gamma;

    double dx = xn * height;
    double dy = yn * height;
    double dz = zn * height;

    return { px + dx, py + dy, pz + dz };
}

void GeoTransform::Init(OGRCoordinateTransformation *pOgrCT, double *Origin)
{
    GeoTransform::pOgrCT = pOgrCT;
    GeoTransform::OriginX = Origin[0];
    GeoTransform::OriginY = Origin[1];
    GeoTransform::OriginZ = Origin[2];
    glm::dvec3 origin = { GeoTransform::OriginX, GeoTransform::OriginY, GeoTransform::OriginZ };
    glm::dvec3 origin_cartographic = origin;
    pOgrCT->Transform(1, &origin_cartographic.x, &origin_cartographic.y, &origin_cartographic.z);
    glm::dmat4 EnuToEcefMatrix = GeoTransform::CalcEnuToEcefMatrix(origin_cartographic.x, origin_cartographic.y, origin_cartographic.z);
    GeoTransform::EcefToEnuMatrix = glm::inverse(EnuToEcefMatrix);
}
