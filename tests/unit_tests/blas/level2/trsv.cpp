/*******************************************************************************
* Copyright 2020-2021 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions
* and limitations under the License.
*
*
* SPDX-License-Identifier: Apache-2.0
*******************************************************************************/

#include <algorithm>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

#include <CL/sycl.hpp>
#include "cblas.h"
#include "oneapi/mkl.hpp"
#include "oneapi/mkl/detail/config.hpp"
#include "onemkl_blas_helper.hpp"
#include "reference_blas_templates.hpp"
#include "test_common.hpp"
#include "test_helper.hpp"

#include <gtest/gtest.h>

using namespace cl::sycl;
using std::vector;

extern std::vector<cl::sycl::device*> devices;

namespace {

template <typename fp>
int test(device* dev, oneapi::mkl::layout layout, oneapi::mkl::uplo upper_lower,
         oneapi::mkl::transpose transa, oneapi::mkl::diag unit_nonunit, int n, int incx, int lda) {
    // Prepare data.
    vector<fp> x, x_ref, A;
    rand_vector(x, n, incx);
    x_ref = x;
    rand_trsm_matrix(A, layout, transa, n, n, lda);

    // Call Reference TRSV.
    const int n_ref = n, incx_ref = incx, lda_ref = lda;
    using fp_ref = typename ref_type_info<fp>::type;

    ::trsv(convert_to_cblas_layout(layout), convert_to_cblas_uplo(upper_lower),
           convert_to_cblas_trans(transa), convert_to_cblas_diag(unit_nonunit), &n_ref,
           (fp_ref*)A.data(), &lda_ref, (fp_ref*)x_ref.data(), &incx_ref);

    // Call DPC++ TRSV.

    // Catch asynchronous exceptions.
    auto exception_handler = [](exception_list exceptions) {
        for (std::exception_ptr const& e : exceptions) {
            try {
                std::rethrow_exception(e);
            }
            catch (exception const& e) {
                std::cout << "Caught asynchronous SYCL exception during TRSV:\n"
                          << e.what() << std::endl;
                print_error_code(e);
            }
        }
    };

    queue main_queue(*dev, exception_handler);

    buffer<fp, 1> x_buffer = make_buffer(x);
    buffer<fp, 1> A_buffer = make_buffer(A);

    try {
#ifdef CALL_RT_API
        switch (layout) {
            case oneapi::mkl::layout::column_major:
                oneapi::mkl::blas::column_major::trsv(main_queue, upper_lower, transa, unit_nonunit,
                                                      n, A_buffer, lda, x_buffer, incx);
                break;
            case oneapi::mkl::layout::row_major:
                oneapi::mkl::blas::row_major::trsv(main_queue, upper_lower, transa, unit_nonunit, n,
                                                   A_buffer, lda, x_buffer, incx);
                break;
            default: break;
        }
#else
        switch (layout) {
            case oneapi::mkl::layout::column_major:
                TEST_RUN_CT_SELECT(main_queue, oneapi::mkl::blas::column_major::trsv, upper_lower,
                                   transa, unit_nonunit, n, A_buffer, lda, x_buffer, incx);
                break;
            case oneapi::mkl::layout::row_major:
                TEST_RUN_CT_SELECT(main_queue, oneapi::mkl::blas::row_major::trsv, upper_lower,
                                   transa, unit_nonunit, n, A_buffer, lda, x_buffer, incx);
                break;
            default: break;
        }
#endif
    }
    catch (exception const& e) {
        std::cout << "Caught synchronous SYCL exception during TRSV:\n" << e.what() << std::endl;
        print_error_code(e);
    }

    catch (const oneapi::mkl::unimplemented& e) {
        return test_skipped;
    }

    catch (const std::runtime_error& error) {
        std::cout << "Error raised during execution of TRSV:\n" << error.what() << std::endl;
    }

    // Compare the results of reference implementation and DPC++ implementation.
    auto x_accessor = x_buffer.template get_access<access::mode::read>();
    bool good = check_equal_trsv_vector(x_accessor, x_ref, n, incx, n, std::cout);

    return (int)good;
}

class TrsvTests
        : public ::testing::TestWithParam<std::tuple<cl::sycl::device*, oneapi::mkl::layout>> {};

TEST_P(TrsvTests, RealSinglePrecision) {
    EXPECT_TRUEORSKIP(test<float>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                  oneapi::mkl::uplo::lower, oneapi::mkl::transpose::nontrans,
                                  oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<float>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                  oneapi::mkl::uplo::upper, oneapi::mkl::transpose::nontrans,
                                  oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<float>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                  oneapi::mkl::uplo::lower, oneapi::mkl::transpose::trans,
                                  oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<float>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                  oneapi::mkl::uplo::upper, oneapi::mkl::transpose::trans,
                                  oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<float>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                  oneapi::mkl::uplo::lower, oneapi::mkl::transpose::nontrans,
                                  oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<float>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                  oneapi::mkl::uplo::upper, oneapi::mkl::transpose::nontrans,
                                  oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<float>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                  oneapi::mkl::uplo::lower, oneapi::mkl::transpose::trans,
                                  oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<float>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                  oneapi::mkl::uplo::upper, oneapi::mkl::transpose::trans,
                                  oneapi::mkl::diag::nonunit, 30, 2, 42));
}
TEST_P(TrsvTests, RealDoublePrecision) {
    EXPECT_TRUEORSKIP(test<double>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                   oneapi::mkl::uplo::lower, oneapi::mkl::transpose::nontrans,
                                   oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<double>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                   oneapi::mkl::uplo::upper, oneapi::mkl::transpose::nontrans,
                                   oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<double>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                   oneapi::mkl::uplo::lower, oneapi::mkl::transpose::trans,
                                   oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<double>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                   oneapi::mkl::uplo::upper, oneapi::mkl::transpose::trans,
                                   oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<double>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                   oneapi::mkl::uplo::lower, oneapi::mkl::transpose::nontrans,
                                   oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<double>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                   oneapi::mkl::uplo::upper, oneapi::mkl::transpose::nontrans,
                                   oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<double>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                   oneapi::mkl::uplo::lower, oneapi::mkl::transpose::trans,
                                   oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<double>(std::get<0>(GetParam()), std::get<1>(GetParam()),
                                   oneapi::mkl::uplo::upper, oneapi::mkl::transpose::trans,
                                   oneapi::mkl::diag::nonunit, 30, 2, 42));
}
TEST_P(TrsvTests, ComplexSinglePrecision) {
    EXPECT_TRUEORSKIP(test<std::complex<float>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::lower,
        oneapi::mkl::transpose::nontrans, oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<float>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::upper,
        oneapi::mkl::transpose::nontrans, oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<float>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::lower,
        oneapi::mkl::transpose::trans, oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<float>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::upper,
        oneapi::mkl::transpose::trans, oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<float>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::lower,
        oneapi::mkl::transpose::conjtrans, oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<float>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::upper,
        oneapi::mkl::transpose::conjtrans, oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<float>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::lower,
        oneapi::mkl::transpose::nontrans, oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<float>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::upper,
        oneapi::mkl::transpose::nontrans, oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<float>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::lower,
        oneapi::mkl::transpose::trans, oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<float>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::upper,
        oneapi::mkl::transpose::trans, oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<float>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::lower,
        oneapi::mkl::transpose::conjtrans, oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<float>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::upper,
        oneapi::mkl::transpose::conjtrans, oneapi::mkl::diag::nonunit, 30, 2, 42));
}
TEST_P(TrsvTests, ComplexDoublePrecision) {
    EXPECT_TRUEORSKIP(test<std::complex<double>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::lower,
        oneapi::mkl::transpose::nontrans, oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<double>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::upper,
        oneapi::mkl::transpose::nontrans, oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<double>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::lower,
        oneapi::mkl::transpose::trans, oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<double>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::upper,
        oneapi::mkl::transpose::trans, oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<double>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::lower,
        oneapi::mkl::transpose::conjtrans, oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<double>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::upper,
        oneapi::mkl::transpose::conjtrans, oneapi::mkl::diag::unit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<double>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::lower,
        oneapi::mkl::transpose::nontrans, oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<double>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::upper,
        oneapi::mkl::transpose::nontrans, oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<double>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::lower,
        oneapi::mkl::transpose::trans, oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<double>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::upper,
        oneapi::mkl::transpose::trans, oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<double>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::lower,
        oneapi::mkl::transpose::conjtrans, oneapi::mkl::diag::nonunit, 30, 2, 42));
    EXPECT_TRUEORSKIP(test<std::complex<double>>(
        std::get<0>(GetParam()), std::get<1>(GetParam()), oneapi::mkl::uplo::upper,
        oneapi::mkl::transpose::conjtrans, oneapi::mkl::diag::nonunit, 30, 2, 42));
}

INSTANTIATE_TEST_SUITE_P(TrsvTestSuite, TrsvTests,
                         ::testing::Combine(testing::ValuesIn(devices),
                                            testing::Values(oneapi::mkl::layout::column_major,
                                                            oneapi::mkl::layout::row_major)),
                         ::LayoutDeviceNamePrint());

} // anonymous namespace