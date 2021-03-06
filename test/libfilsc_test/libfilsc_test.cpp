// libfilsc_test.cpp : Defines the entry point for the console application.
//

/// <summary>
/// Test program for 'libfilsc' library.
/// </summary>

#include "libfilsc_test_pch.h"

using namespace std;

string loadTestFilter();

int main(int argc, char **argv)
{
    string filter = loadTestFilter();

    if (filter != "")
        ::testing::GTEST_FLAG(filter) = filter;

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

/// <summary>
/// Load test filter file.
/// </summary>
/// <returns>Text filter or an empty string if not found</returns>
string loadTestFilter()
{
    string content = readTextFile("test_filter.txt");

    //Transform Windows end line markers to Unix end line markers.
    replaceIn(content, '\r', "");

    auto lines = split(content, "\n");

    if (lines.size() > 0)
        return trim(lines[0]);
    else
        return "";
}