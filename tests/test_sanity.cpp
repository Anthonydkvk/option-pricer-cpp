#include <gtest/gtest.h>

#include "pricer/version.hpp"

// Test factice : vérifie que GoogleTest tourne et que la bibliothèque pricer est bien liée.
TEST(Sanity, GoogleTestRuns) {
    EXPECT_EQ(1 + 1, 2);
}

TEST(Sanity, LibraryIsLinked) {
    EXPECT_EQ(pricer::version(), "0.1.0");
}
