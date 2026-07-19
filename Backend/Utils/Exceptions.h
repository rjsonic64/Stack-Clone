#pragma once

#include <atomic>
#include <cstddef>



using ErrorData = struct errs
{
	size_t total_rows = 0;
	size_t discarded_rows = 0;
	
	size_t bad_interger = 0;
	size_t bad_date = 0;
	size_t malformed_row = 0;

	double elapsed_time = 0.0;
};
