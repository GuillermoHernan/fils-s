// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <iostream>
#include <fstream>

#include <functional>
#include <vector>
#include <map>

#include <filesystem>
namespace fs = std::experimental::filesystem;

#include <memory>

#include "gtest/gtest.h"
#include "testUtils.h"
#include "utils.h"

