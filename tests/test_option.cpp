#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

#include "pricer/option.hpp"

using namespace pricer;

namespace {
const double not_a_number = std::numeric_limits<double>::quiet_NaN();
}

TEST(OptionValidation, ValidInputsDoNotThrow) {
    EXPECT_NO_THROW(validate(MarketParams{100.0, 0.05, 0.0, 0.2}));
    EXPECT_NO_THROW(validate(Option{OptionType::Call, ExerciseStyle::European, 100.0, 1.0}));
    // Taux et dividende négatifs sont légitimes (ex. taux négatifs en EUR).
    EXPECT_NO_THROW(validate(MarketParams{100.0, -0.01, -0.02, 0.2}));
}

TEST(OptionValidation, InvalidMarketThrows) {
    EXPECT_THROW(validate(MarketParams{0.0, 0.05, 0.0, 0.2}), std::invalid_argument);    // S = 0
    EXPECT_THROW(validate(MarketParams{-100.0, 0.05, 0.0, 0.2}), std::invalid_argument); // S < 0
    EXPECT_THROW(validate(MarketParams{100.0, 0.05, 0.0, 0.0}), std::invalid_argument);  // sigma = 0
    EXPECT_THROW(validate(MarketParams{100.0, 0.05, 0.0, -0.2}), std::invalid_argument); // sigma < 0
    EXPECT_THROW(validate(MarketParams{not_a_number, 0.05, 0.0, 0.2}), std::invalid_argument);    // S = NaN
}

TEST(OptionValidation, InvalidOptionThrows) {
    EXPECT_THROW(validate(Option{OptionType::Put, ExerciseStyle::European, 0.0, 1.0}), std::invalid_argument);
    EXPECT_THROW(validate(Option{OptionType::Put, ExerciseStyle::European, -5.0, 1.0}), std::invalid_argument);
    EXPECT_THROW(validate(Option{OptionType::Put, ExerciseStyle::European, 100.0, 0.0}), std::invalid_argument);
    EXPECT_THROW(validate(Option{OptionType::Put, ExerciseStyle::European, 100.0, -1.0}), std::invalid_argument);
    EXPECT_THROW(validate(Option{OptionType::Put, ExerciseStyle::European, 100.0, not_a_number}), std::invalid_argument);
}
