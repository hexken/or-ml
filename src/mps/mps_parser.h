
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

#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Row constraint types
enum class RowType {
  N, // Free row
  G, // Greater than or equal
  L, // Less than or equal
  E, // Equality
};

// Variable bound types
enum class BoundType {
  UP, // Upper
  LO, // Lower
  FX, // Fixed (upper = lower)
  FR, // Free
  MI, // Minus infinity
  PL, // Plus infinity
  BV, // Binary variable
  LI, // Integer lower
  SC, // Semi-continuous (either zero or >= lower bound)
  UI  // Integer upper
};

namespace detail {

template <typename Map, typename Key>
auto at_or_throw(Map &m, Key &&k) -> typename Map::mapped_type & {
  auto it = m.find(std::forward<Key>(k));
  if (it == m.end())
    throw std::out_of_range("key not found");
  return it->second;
}

struct StringHash {
  using hash_type = std::hash<std::string_view>;
  using is_transparent = void;

  std::size_t operator()(const char *str) const { return hash_type{}(str); }
  std::size_t operator()(std::string_view str) const {
    return hash_type{}(str);
  }
  std::size_t operator()(const std::string &str) const {
    return hash_type{}(str);
  }
};

using StringIntMap =
    std::unordered_map<std::string, int, StringHash, std::equal_to<>>;
} // namespace detail
//
struct ParsedMPS {
  std::string name;
  int num_rows{};
  int num_cols{};

  int obj_row{-1};
  std::string obj_name;
  // we will always minimize internally
  std::string obj_sense;

  std::string rhs_name;
  std::string bound_name;

  // Indices for rows and columns
  std::vector<std::string> col_names;
  std::vector<std::string> row_names;
  detail::StringIntMap col_indices;
  detail::StringIntMap row_indices;
  // std::unordered_map<std::string, int, std::hash<std::string_view>,
  //                    std::equal_to<>>
  //     col_index;
  // std::unordered_map<std::string, int> row_index;

  // Objective coefficients
  std::vector<double> obj_coefficients;

  // Constraint types and RHS std::vector
  std::vector<RowType> row_type;
  std::vector<double> rhs;

  // Contraint matrix A in COO format
  std::vector<int> A_row;
  std::vector<int> A_col;
  std::vector<double> A_val;

  // Variable bounds
  std::vector<double> lower_bounds;
  std::vector<double> upper_bounds;
  // std::vector<bool> is_integer;
  // std::vector<bool> is_binary;
};

ParsedMPS parse_mps(const std::string_view filename);

#endif
