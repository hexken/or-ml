#include "mps_parser.h"
#include <Eigen/Dense>
#include <iostream>

int main() {
  std::string filename{"tests/instances/chip.mps"};
  auto prob = mps::parse_file(filename);
  Eigen::MatrixXd A(2, 2);
  A << 1, 2, 3, 4;
  std::cout << "A = \n" << A << "\n";
  std::cout << "A.inverse() = \n" << A.inverse() << "\n";

  return 0;
}
