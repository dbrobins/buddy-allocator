/*
   Test harness.

   D. Robins, 20141012
*/

#include "gtest/gtest.h"


int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

