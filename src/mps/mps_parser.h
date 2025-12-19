// Copyright 2025 Kenneth Tjhia
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// A simple MPS parser that does minimal validation and stores the data in a
// struct.
// Supports a subset of MPS (CPLEX extensions) free-format.
// Supports NAME, OBJNAME, OBJSENSE, ROWS, COLUMNS, RHS, BOUNDS sections.
// Follows convention on OBJNAME, OBJSENSE, RHS, and BOUNDS being optional.
// Assumes sections in that order.
// Identifers are assumed to be alphanumeric and underscores only.
// Does not currently support integer or binary variables.
// If multiple BOUNDS or RHS vectors are specified, only the first is stored.
// If no RHS is specified for a row, defaults to 0.0.
// If no BOUNDS are specified for a variable, defaults to free (-inf, inf).
// Of course, COLUMNS only specifies nonzero entries.

// TODO: add support for integer and binary variables.
#ifndef mps_parser_h
#define mps_parser_h

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mps {

// Assuming this is defined for the target platform
constexpr double infinity = std::numeric_limits<double>::infinity();

struct StringHash {
  using hash_type = std::hash<std::string_view>;
  using is_transparent = void;

  std::size_t operator()(std::string_view str) const {
    return hash_type{}(str);
  }
  // These overloads probably aren't necessary, but whatever.
  std::size_t operator()(const char *str) const { return hash_type{}(str); }
  std::size_t operator()(const std::string &str) const {
    return hash_type{}(str);
  }
};

using StringIntMap =
    std::unordered_map<std::string, int, StringHash, std::equal_to<>>;

enum class SectionType {
  None,
  Name,
  ObjectiveName,
  ObjectiveSense,
  Rows,
  Columns,
  Rhs,
  Bounds,
  EndData
};

enum class ObjectiveSense { Min, Max };

// Row constraint types
enum class RowType {
  N, // Free row
  G, // Greater than or equal
  L, // Less than or equal
  E, // Equality
};

// Variable bound types
enum class BoundType {
  Up, // Upper
  Lo, // Lower
  Fx, // Fixed (upper = lower)
  Fr, // Free
  Mi, // Minus infinity
  Pl, // Plus infinity
  Bv, // Binary variable
  Li, // Integer lower
  Sc, // Semi-continuous (either zero or >= lower bound)
  Ui  // Integer upper
};

struct VariableBounds {
  double lower = -infinity;
  double upper = infinity;
};

struct MatrixEntry {
  std::size_t row;
  std::size_t column;
  double value;
};

struct ParsedMps {
  std::optional<std::string> name{std::nullopt};
  std::optional<std::string> objective_name{std::nullopt};
  std::optional<std::string> rhs_name{std::nullopt};
  std::optional<std::string> bound_name{std::nullopt};

  // we will always convert to min, but record here the original sense
  ObjectiveSense objective_sense{ObjectiveSense::Min};

  std::size_t num_rows{};
  std::size_t num_cols{};

  std::vector<std::string> column_names;
  std::vector<std::string> row_names;
  StringIntMap column_indices;
  StringIntMap row_indices;

  std::vector<RowType> row_types;
  std::vector<double> objective_coeffs;
  std::vector<MatrixEntry> matrix_entries;
  std::vector<double> rhs_values;
  std::vector<VariableBounds> variable_bounds;

  bool validate() const;
  void convert_to_minimization();
};

ParsedMps parse_file(const std::string &filename);

} // namespace mps
#endif
