#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest/doctest.h"
#include "mps_parser.h"
#include <doctest/doctest.h>

TEST_CASE("Parse chip.mps correctly") {
  // NAME          CHIP
  // ROWS
  //  L ASSEMBLY
  //  L FINISHNG
  //  N INCOME
  // COLUMNS
  //     P1        ASSEMBLY      1.0        FINISHNG    1.0
  //     P1      INCOME        -10.0
  //     P2        ASSEMBLY      2.0        FINISHNG    4.0
  //     P2      INCOME        -25.0
  // RHS
  //     RESOURCES ASSEMBLY     80.0        FINISHNG  120.0
  // ENDATA

  const std::string filename = "tests/instances/chip.mps";
  mps::ParsedMps parsed = mps::parse_file(filename);

  // Validate overall structure
  // Check name fields
  REQUIRE(parsed.name.has_value());
  CHECK(*parsed.name == "CHIP");
  CHECK_FALSE(parsed.objective_name.has_value());
  CHECK(parsed.rhs_name.has_value());
  CHECK(*parsed.rhs_name == "RESOURCES");
  CHECK_FALSE(parsed.bound_name.has_value());

  // Objective sense defaults to Min; problem is max (INCOME row is N), so
  // original sense should be Max
  CHECK(parsed.objective_sense == mps::ObjectiveSense::Max);

  // Counts
  CHECK(parsed.num_rows == 3);
  CHECK(parsed.num_cols == 2);

  // Row names and types
  REQUIRE(parsed.row_names.size() == 3);
  CHECK(parsed.row_names[0] == "ASSEMBLY");
  CHECK(parsed.row_names[1] == "FINISHNG");
  CHECK(parsed.row_names[2] == "INCOME");

  REQUIRE(parsed.row_types.size() == 3);
  CHECK(parsed.row_types[0] == mps::RowType::L);
  CHECK(parsed.row_types[1] == mps::RowType::L);
  CHECK(parsed.row_types[2] == mps::RowType::N);

  // Column names
  REQUIRE(parsed.column_names.size() == 2);
  CHECK(parsed.column_names[0] == "P1");
  CHECK(parsed.column_names[1] == "P2");

  // Name-to-index maps
  REQUIRE(parsed.row_indices.at("ASSEMBLY") == 0);
  REQUIRE(parsed.row_indices.at("FINISHNG") == 1);
  REQUIRE(parsed.row_indices.at("INCOME") == 2);
  REQUIRE(parsed.column_indices.at("P1") == 0);
  REQUIRE(parsed.column_indices.at("P2") == 1);

  // Objective coefficients (note: objective row is INCOME, which has -10 and
  // -25)
  REQUIRE(parsed.objective_coefficients.size() == 2);
  CHECK(parsed.objective_coefficients[0] == -10.0); // P1
  CHECK(parsed.objective_coefficients[1] == -25.0); // P2

  // RHS values
  REQUIRE(parsed.rhs_values.size() == 3);
  CHECK(parsed.rhs_values[0] == 80.0);  // ASSEMBLY
  CHECK(parsed.rhs_values[1] == 120.0); // FINISHNG
  CHECK(parsed.rhs_values[2] == 0.0); // INCOME (not in RHS section → default 0)

  // Variable bounds (default: free)
  REQUIRE(parsed.variable_bounds.size() == 2);
  const auto inf = mps::infinity;
  CHECK(parsed.variable_bounds[0].lower == -inf);
  CHECK(parsed.variable_bounds[0].upper == inf);
  CHECK(parsed.variable_bounds[1].lower == -inf);
  CHECK(parsed.variable_bounds[1].upper == inf);

  // Matrix entries (should include all constraint coefficients, but NOT
  // objective row) Note: depending on your parser design, you may or may not
  // include objective row in matrix_entries. In standard MPS, the objective row
  // (N) is often excluded from constraint matrix. Here, ASSEMBLY and FINISHNG
  // are constraints; INCOME is objective → not in matrix_entries.

  // Expected non-objective entries:
  // P1 → ASSEMBLY: 1.0, FINISHNG: 1.0
  // P2 → ASSEMBLY: 2.0, FINISHNG: 4.0
  REQUIRE(parsed.matrix_entries.size() == 4);

  // Helper to find entry
  auto find_entry = [&](size_t row, size_t col) -> std::optional<double> {
    for (const auto &e : parsed.matrix_entries) {
      if (e.row == row && e.column == col)
        return e.value;
    }
    return std::nullopt;
  };

  CHECK(find_entry(0, 0) == 1.0); // P1 in ASSEMBLY
  CHECK(find_entry(1, 0) == 1.0); // P1 in FINISHNG
  CHECK(find_entry(0, 1) == 2.0); // P2 in ASSEMBLY
  CHECK(find_entry(1, 1) == 4.0); // P2 in FINISHNG

  // Optional: verify convert_to_minimization flips signs
  // parsed.convert_to_minimization();
  CHECK(parsed.objective_coefficients[0] == 10.0);
  CHECK(parsed.objective_coefficients[1] == 25.0);
}
