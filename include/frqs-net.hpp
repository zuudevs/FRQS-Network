#pragma once

/**
 * @file frqs-net.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "core/server.hpp"

#ifdef ERROR
	#undef ERROR
#endif

#include "utils/logger.hpp"

#define MAJOR 1
#define MINOR 0
#define PATCH 0

namespace frqs {
	static constexpr uint32_t VERSION_CODE = 100 ;
	static constexpr const char* VERSION = "1.0.0" ;
} // namespace frqs
