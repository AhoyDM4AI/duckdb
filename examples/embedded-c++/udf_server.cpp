#include "duckdb.hpp"
#include "duckdb/common/arrow/arrow_transform_util.hpp"

#include <arrow/python/pyarrow.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace duckdb;
using namespace imbridge;

int main(int argc, char **argv) {

	if (argc < 2) {
		std::cout << "[Server] you shoule add a parameter\n";
		return 0;
	}
	std::string channel_name = argv[1];
	// std::cout << "[Server] start " << channel_name << " server\n";
	if (!Py_IsInitialized()) {
		Py_Initialize();
	}
	PyGILState_STATE gstate;
	gstate = PyGILState_Ensure();
	if (arrow::py::import_pyarrow()) {
		std::cout
		    << "[Server] import pyarrow error! make sure your default python environment has installed the pyarrow\n";
		exit(0);
	}

	imbridge::SharedMemoryManager shm_server(channel_name, imbridge::ProcessKind::SERVER);

	// prepare the environment
	std::ifstream file("/root/workspace/duckdb/examples/embedded-c++/code.py");
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string python_code = buffer.str();
	PyRun_SimpleString(python_code.c_str());
	PyObject *main_module = PyImport_AddModule("__main__");
	PyObject *main_dict = PyModule_GetDict(main_module);
	PyObject *MyProcess = PyDict_GetItemString(main_dict, "MyProcess");
	PyObject *my_process_instance = PyObject_CallObject(MyProcess, NULL);

	while (true) {
		shm_server.sem_server->wait();
		if (!shm_server.is_alive()) {
			break;
		}
		std::shared_ptr<arrow::Table> my_table = imbridge::ReadArrowTableFromSharedMemory(shm_server, INPUT_TABLE);

		PyObject *py_table_tmp = arrow::py::wrap_table(std::move(my_table));
		PyObject *py_result = PyObject_CallMethod(my_process_instance, "process", "O", py_table_tmp);

		if (py_result != NULL) {
			my_table = arrow::py::unwrap_table(py_result).ValueOrDie();
		}

		imbridge::WriteArrowTableToSharedMemory(my_table, shm_server, OUTPUT_TABLE);
		shm_server.sem_client->post();
	}
	// std::cout << "[Server] udf server " << channel_name << " closed\n";
	shm_server.sem_client->post();
	return 0;
}