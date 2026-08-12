#include "Database/database.h"

#include <pybind11/pybind11.h>



PYBIND11_MODULE(stackclone_module, m)
{
	// Functions
	
	// Variables/Classes
	pybind11::class_<Database::MySql>(m, "db_MySql").def(pybind11::init<std::string>()).def("get_post", &Database::MySql::get_post).def("get_session", &Database::MySql::getSession);
}
