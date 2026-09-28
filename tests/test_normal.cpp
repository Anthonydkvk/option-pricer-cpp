#include <gtest/gtest.h>

#include <cmath>

#include "pricer/normal.hpp"

using pricer::normal_cdf;
using pricer::normal_pdf;

TEST(Normal, CdfKnownValues) {
    EXPECT_DOUBLE_EQ(normal_cdf(0.0), 0.5);
    EXPECT_NEAR(normal_cdf(1.96), 0.9750021048517795, 1e-12);
    EXPECT_NEAR(normal_cdf(-1.0), 0.15865525393145707, 1e-12);
}

TEST(Normal, CdfSymmetry) {
    for (const double x : {0.1, 0.5, 1.0, 2.5, 4.0}) {
        EXPECT_NEAR(normal_cdf(x) + normal_cdf(-x), 1.0, 1e-15);
    }
}

TEST(Normal, CdfTailsAreAccurate) {
    // Queue gauche lointaine : erfc garde de la précision relative là où 1 + erf s'annulerait.
    EXPECT_GT(normal_cdf(-10.0), 0.0);
    EXPECT_NEAR(normal_cdf(-10.0) / 7.619853024160527e-24, 1.0, 1e-10);
}

TEST(Normal, PdfKnownValues) {
    EXPECT_NEAR(normal_pdf(0.0), 0.3989422804014327, 1e-15);
    EXPECT_NEAR(normal_pdf(1.0), 0.24197072451914337, 1e-15);
    EXPECT_DOUBLE_EQ(normal_pdf(1.3), normal_pdf(-1.3));
}
