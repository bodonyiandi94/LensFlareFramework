/*################################################################################
  ##
  ##   Copyright (C) 2016-2023 Keith O'Hara
  ##
  ##   This file is part of the BaseMatrixOps C++ library.
  ##
  ##   Licensed under the Apache License, Version 2.0 (the "License");
  ##   you may not use this file except in compliance with the License.
  ##   You may obtain a copy of the License at
  ##
  ##       http://www.apache.org/licenses/LICENSE-2.0
  ##
  ##   Unless required by applicable law or agreed to in writing, software
  ##   distributed under the License is distributed on an "AS IS" BASIS,
  ##   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  ##   See the License for the specific language governing permissions and
  ##   limitations under the License.
  ##
  ################################################################################*/

#define BMO_ENABLE_EXTRA_FEATURES
#define BMO_ENABLE_EXTRA_EXPERIMENTAL
#include "bmo_tests.hpp"

int main()
{
    bmo::Cube_t<double> test_cube_1(4,2,3);

    bmo::Mat_t test_mat_1 = test_cube_1.mat(0);
    
    BMO_MATOPS_COUT << test_mat_1 << BMO_MATOPS_ENDL;

    const bmo::Mat_t& test_mat_2 = test_cube_1.mat(1);
    
    BMO_MATOPS_COUT << test_mat_2 << BMO_MATOPS_ENDL;

    double elem_val_1 = test_cube_1(1,0,1);

    std::cout << "elem_val_1 = " << elem_val_1 << std::endl;

    double& elem_val_2 = test_cube_1(1,0,1);

    std::cout << "elem_val_2 = " << elem_val_2 << std::endl;

    elem_val_2 = 1;

    std::cout << "elem_val_2 = " << elem_val_2 << ". test_cube_1(1,0,1) = " << test_cube_1(1,0,1) << std::endl;

    //

    bmo::Cube_t<double> test_cube_2(std::move(test_cube_1));

    std::cout << "test_cube_1.n_row = " << test_cube_1.n_row << std::endl;
    std::cout << "test_cube_2.n_row = " << test_cube_2.n_row << std::endl;

    bmo::Cube_t<double> test_cube_3 = std::move(test_cube_2);

    std::cout << "test_cube_2.n_row = " << test_cube_2.n_row << std::endl;
    std::cout << "test_cube_3.n_row = " << test_cube_3.n_row << std::endl;

    bmo::Cube_t<double> test_cube_4 = test_cube_3;

    std::cout << "test_cube_3.n_row = " << test_cube_3.n_row << std::endl;
    std::cout << "test_cube_4.n_row = " << test_cube_4.n_row << std::endl;

    //

    test_cube_1.setZero(6,4,3);

    test_mat_1 = test_cube_1.mat(1);
    
    BMO_MATOPS_COUT << test_mat_1 << BMO_MATOPS_ENDL;

    return 0;
}
 