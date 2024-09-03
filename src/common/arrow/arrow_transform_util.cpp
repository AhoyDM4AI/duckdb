#include "duckdb/common/arrow/arrow_transform_util.hpp"

#include "duckdb/common/arrow/arrow_appender.hpp"
#include "duckdb/common/arrow/arrow_converter.hpp"

#include <arrow/array/concatenate.h>
#include <arrow/c/abi.h>
#include <arrow/c/bridge.h>
#include <arrow/c/helpers.h>
#include <arrow/io/api.h>
#include <arrow/io/memory.h>
#include <arrow/ipc/api.h>
#include <arrow/ipc/writer.h>
#include <arrow/status.h>
#include <string>
#include <thread>

namespace duckdb {

namespace imbridge {

std::shared_ptr<arrow::Table> ConvertDataChunkToArrowTable(DataChunk &input, const ClientProperties &options) {
	auto types = input.GetTypes();
	vector<string> names;
	names.reserve(types.size());

	for (idx_t i = 0; i < types.size(); i++) {
		names.push_back(StringUtil::Format("c%d", i));
	}

	ArrowSchema schema;
	ArrowConverter::ToArrowSchema(&schema, types, names, options);

	idx_t init_capacity = input.size() > STANDARD_VECTOR_SIZE ? NextPowerOfTwo(input.size()) : STANDARD_VECTOR_SIZE;
	ArrowAppender appender(types, init_capacity, options);
	appender.Append(input, 0, input.size(), input.size());
	ArrowArray array = appender.Finalize();

	std::shared_ptr<arrow::Schema> arrow_schema;
	std::vector<std::shared_ptr<arrow::Array>> arrow_array;

	for (int i = 0; i < array.n_children; i++) {
		arrow_array.emplace_back(arrow::ImportArray(array.children[i], schema.children[i]).ValueOrDie());
	}

	ArrowSchema schema1;
	ArrowConverter::ToArrowSchema(&schema1, types, names, options);

	arrow_schema = arrow::ImportSchema(&schema1).ValueOrDie();

	std::shared_ptr<arrow::Table> table = arrow::Table::Make(arrow_schema, arrow_array);
	return table;
}

void WriteArrowTableToSharedMemory(std::shared_ptr<arrow::Table> &table, SharedMemoryManager &shm,
                                   const std::string &shm_id) {
	std::shared_ptr<arrow::Buffer> buffer;
	std::shared_ptr<arrow::io::BufferOutputStream> stream =
	    arrow::io::BufferOutputStream::Create(table->num_columns() * table->num_rows() * sizeof(double_t)).ValueOrDie();
	std::shared_ptr<arrow::ipc::RecordBatchWriter> writer =
	    arrow::ipc::MakeStreamWriter(stream, table->schema()).ValueOrDie();
	writer->WriteTable(*table);
	writer->Close();
	buffer = stream->Finish().ValueOrDie();

	char *shm_ptr = shm.create_shared_memory_object<char>(shm_id, buffer->size());
	std::memcpy(shm_ptr, buffer->data(), buffer->size());
}

std::shared_ptr<arrow::Table> ReadArrowTableFromSharedMemory(SharedMemoryManager &shm, const std::string &shm_id) {
	auto shm_table_pair = shm.open_shared_memory_object<char>(shm_id);

	if (shm_table_pair.first == nullptr) {
		throw std::runtime_error("[BOOST SHARED MEMORY] Cannot find shared memory with id: " + shm.get_channel_name() +
		                         shm_id);
		return nullptr;
	}

	char *shm_table_ptr = shm_table_pair.first;
	size_t shm_table_size = shm_table_pair.second;

	std::shared_ptr<arrow::Buffer> buffer = arrow::Buffer::Wrap(shm_table_ptr, shm_table_size);
	std::shared_ptr<arrow::io::InputStream> input = std::make_shared<arrow::io::BufferReader>(buffer);
	std::shared_ptr<arrow::ipc::RecordBatchReader> reader =
	    arrow::ipc::RecordBatchStreamReader::Open(input).ValueOrDie();

	std::vector<std::shared_ptr<arrow::RecordBatch>> batches;
	while (true) {
		std::shared_ptr<arrow::RecordBatch> batch = reader->ReadNext().ValueOrDie().batch;
		if (batch == nullptr) {
			break;
		}
		batches.push_back(batch);
	}

	std::shared_ptr<arrow::Table> table = arrow::Table::FromRecordBatches(reader->schema(), batches).ValueOrDie();
	return table;
}

void  ConvertArrowTableResultToVector(std::shared_ptr<arrow::Table> &table, Vector &res) {
	// As the duckdb_python_udf, UDF only support one column return.
	// only support directyly conver
	std::shared_ptr<arrow::ChunkedArray> column = table->column(0);
	std::vector<std::shared_ptr<arrow::Array>> chunks = column->chunks();
	std::shared_ptr<arrow::Array> array = arrow::Concatenate(chunks).ValueOrDie();
	ArrowArray c_array;
	arrow::Status status = arrow::ExportArray(*array, &c_array);
	auto data_ptr = (data_ptr_t)c_array.buffers[1];
	FlatVector::SetData(res, data_ptr);
}

} // namespace imbridge

} // namespace duckdb