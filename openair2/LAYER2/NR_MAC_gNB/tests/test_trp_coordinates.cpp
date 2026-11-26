/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */

#include "gtest/gtest.h"

extern "C" void test_trp_coordinates(int scenario);

TEST(TrpCoordinates, MissingCoordinatesRejected)
{
  test_trp_coordinates(0);
}

TEST(TrpCoordinates, ConfiguredOriginReported)
{
  test_trp_coordinates(1);
}

TEST(TrpCoordinates, PciWithoutCoordinates)
{
  test_trp_coordinates(2);
}
