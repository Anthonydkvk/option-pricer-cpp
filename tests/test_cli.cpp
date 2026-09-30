#include <gtest/gtest.h>

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "cli.hpp"
#include "pricer/black_scholes.hpp"

using namespace pricer;

namespace {

// Petits réglages pour que les tests restent rapides (le CLI par défaut : 2001 pas, 500 000 trajectoires).
CliConfig fast_config() {
    CliConfig config;
    config.steps = 201;
    config.paths = 20'000;
    return config;
}

std::size_t count_lines(const std::string& text) {
    std::size_t lines = 0;
    for (const char c : text) {
        lines += (c == '\n');
    }
    return lines;
}

}  // namespace

// ---------------------------------------------------------------------------
// Lecture des arguments
// ---------------------------------------------------------------------------
TEST(CliParseArgs, DefaultsAreTheStandardCase) {
    const CliConfig c = parse_args({});
    EXPECT_DOUBLE_EQ(c.market.S, 100.0);
    EXPECT_DOUBLE_EQ(c.option.K, 100.0);
    EXPECT_DOUBLE_EQ(c.market.r, 0.05);
    EXPECT_DOUBLE_EQ(c.market.q, 0.0);
    EXPECT_DOUBLE_EQ(c.market.sigma, 0.2);
    EXPECT_DOUBLE_EQ(c.option.T, 1.0);
    EXPECT_EQ(c.option.type, OptionType::Call);
    EXPECT_EQ(c.option.style, ExerciseStyle::European);
    EXPECT_EQ(c.steps % 2, 1);  // impair : aucun nœud pile sur le strike
    EXPECT_FALSE(c.csv);
    EXPECT_FALSE(c.help);
}

TEST(CliParseArgs, ReadsEveryOption) {
    const CliConfig c = parse_args({"--S", "90", "--K", "110", "--r", "0.03", "--q", "0.01",
                                    "--sigma", "0.25", "--T", "0.5", "--type", "put", "--style",
                                    "american", "--steps", "501", "--tree-bump", "0.05",
                                    "--paths", "1000", "--seed", "7", "--antithetic", "--csv"});
    EXPECT_DOUBLE_EQ(c.market.S, 90.0);
    EXPECT_DOUBLE_EQ(c.option.K, 110.0);
    EXPECT_DOUBLE_EQ(c.market.r, 0.03);
    EXPECT_DOUBLE_EQ(c.market.q, 0.01);
    EXPECT_DOUBLE_EQ(c.market.sigma, 0.25);
    EXPECT_DOUBLE_EQ(c.option.T, 0.5);
    EXPECT_EQ(c.option.type, OptionType::Put);
    EXPECT_EQ(c.option.style, ExerciseStyle::American);
    EXPECT_EQ(c.steps, 501);
    EXPECT_DOUBLE_EQ(c.tree_spot_bump, 0.05);
    EXPECT_EQ(c.paths, 1000);
    EXPECT_EQ(c.seed, 7u);
    EXPECT_TRUE(c.antithetic);
    EXPECT_TRUE(c.csv);
}

TEST(CliParseArgs, Help) {
    EXPECT_TRUE(parse_args({"--help"}).help);
    EXPECT_TRUE(parse_args({"-h"}).help);
}

TEST(CliParseArgs, RejectsMalformedArguments) {
    using Args = std::vector<std::string>;
    for (const Args& bad : {Args{"--unknown"}, Args{"--S"}, Args{"--S", "abc"},
                            Args{"--S", "10x"}, Args{"--steps", "2.5"}, Args{"--seed", "-1"},
                            Args{"--type", "straddle"}, Args{"--style", "bermudan"},
                            Args{"100"}}) {
        EXPECT_THROW(parse_args(bad), std::invalid_argument) << bad.front();
    }
}

// ---------------------------------------------------------------------------
// Calcul
// ---------------------------------------------------------------------------
TEST(CliRunAll, EuropeanRunsFourMethods) {
    const std::vector<MethodResult> results = run_all(fast_config());
    ASSERT_EQ(results.size(), 4u);
    EXPECT_EQ(results[0].id, "bs_analytic");
    EXPECT_EQ(results[1].id, "bs_fd");
    EXPECT_EQ(results[2].id, "crr");
    EXPECT_EQ(results[3].id, "mc");

    EXPECT_NEAR(results[0].price, 10.450584, 1e-6);
    EXPECT_NEAR(results[1].greeks.delta, results[0].greeks.delta, 1e-6);
    EXPECT_NEAR(results[2].price, 10.450584, 2e-2);

    // Seul Monte Carlo a une erreur statistique, et le vrai prix est dans ±3 erreurs standard.
    for (std::size_t i = 0; i < 3; ++i) {
        EXPECT_FALSE(results[i].monte_carlo.has_value()) << results[i].id;
    }
    ASSERT_TRUE(results[3].monte_carlo.has_value());
    EXPECT_LT(std::abs(results[3].price - 10.450584), 3.0 * results[3].monte_carlo->std_error);

    for (const MethodResult& r : results) {
        EXPECT_GE(r.price_us, 0.0) << r.id;
        EXPECT_GE(r.greeks_us, 0.0) << r.id;
    }
}

TEST(CliRunAll, AmericanRunsTreeOnly) {
    CliConfig config = fast_config();
    config.option.type = OptionType::Put;
    config.option.style = ExerciseStyle::American;
    const std::vector<MethodResult> results = run_all(config);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].id, "crr");
    EXPECT_NEAR(results[0].price, 6.0902, 1e-2);
}

TEST(CliRunAll, RejectsInvalidParameters) {
    CliConfig config = fast_config();
    config.market.sigma = -0.2;
    EXPECT_THROW(run_all(config), std::invalid_argument);

    config = fast_config();
    config.paths = 0;
    EXPECT_THROW(run_all(config), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Affichage
// ---------------------------------------------------------------------------
TEST(CliOutput, CsvHasHeaderAndOneLinePerMethod) {
    const std::vector<MethodResult> results = run_all(fast_config());
    std::ostringstream out;
    print_csv(out, results);
    const std::string csv = out.str();

    EXPECT_EQ(csv.rfind("method,price,std_error,delta,gamma,vega,theta,rho,price_us,greeks_us\n", 0),
              0u);
    EXPECT_EQ(count_lines(csv), 1 + results.size());
    EXPECT_NE(csv.find("\nbs_analytic,10.4505835722,,"), std::string::npos) << csv;
}

TEST(CliOutput, TableListsEveryMethod) {
    const CliConfig config = fast_config();
    const std::vector<MethodResult> results = run_all(config);
    std::ostringstream out;
    print_table(out, config, results);
    const std::string table = out.str();
    for (const MethodResult& r : results) {
        EXPECT_NE(table.find(r.label), std::string::npos) << r.label;
    }
    EXPECT_NE(table.find("95 % CI"), std::string::npos);
}
