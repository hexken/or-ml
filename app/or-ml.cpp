#include "mps_parser.h"
#include <Eigen/Dense>
#include <iostream>

int main() {
  auto prob = mps::parse_mps("tests/instances/chip.mps");
  Eigen::MatrixXd A(2, 2);
  A << 1, 2, 3, 4;
  std::cout << "A = \n" << A << "\n";
  std::cout << "A.inverse() = \n" << A.inverse() << "\n";

  return 0;
}
