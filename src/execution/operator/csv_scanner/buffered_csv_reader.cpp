#include "duckdb/execution/operator/persistent/csv_scanner/buffered_csv_reader.hpp"

#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/to_string.hpp"
#include "duckdb/common/types/cast_helpers.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/execution/operator/persistent/csv_scanner/csv_state_machine.hpp"
#include "duckdb/function/scalar/strftime_format.hpp"
#include "duckdb/main/client_data.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/error_manager.hpp"
#include "duckdb/parser/column_definition.hpp"
#include "duckdb/parser/keyword_helper.hpp"
#include "duckdb/storage/data_table.hpp"
#include "utf8proc.hpp"
#include "utf8proc_wrapper.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>

namespace duckdb {

BufferedCSVReader::BufferedCSVReader(ClientContext &context, BufferedCSVReaderOptions options_p,
                                     const vector<LogicalType> &requested_types)
    : BaseCSVReader(context, std::move(options_p), requested_types), buffer_size(0), position(0), start(0) {
	file_handle = OpenCSV(options);
	Initialize(requested_types);
}

BufferedCSVReader::BufferedCSVReader(ClientContext &context, string filename, BufferedCSVReaderOptions options_p,
                                     const vector<LogicalType> &requested_types)
    : BaseCSVReader(context, std::move(options_p), requested_types), buffer_size(0), position(0), start(0) {
	options.file_path = std::move(filename);
	file_handle = OpenCSV(options);
	Initialize(requested_types);
}

TextSearchShiftArray::TextSearchShiftArray() {
}

TextSearchShiftArray::TextSearchShiftArray(string search_term) : length(search_term.size()) {
	if (length > 255) {
		throw InvalidInputException("Size of delimiter/quote/escape in CSV reader is limited to 255 bytes");
	}
	// initialize the shifts array
	shifts = unique_ptr<uint8_t[]>(new uint8_t[length * 255]);
	memset(shifts.get(), 0, length * 255 * sizeof(uint8_t));
	// iterate over each of the characters in the array
	for (idx_t main_idx = 0; main_idx < length; main_idx++) {
		uint8_t current_char = (uint8_t)search_term[main_idx];
		// now move over all the remaining positions
		for (idx_t i = main_idx; i < length; i++) {
			bool is_match = true;
			// check if the prefix matches at this position
			// if it does, we move to this position after encountering the current character
			for (idx_t j = 0; j < main_idx; j++) {
				if (search_term[i - main_idx + j] != search_term[j]) {
					is_match = false;
				}
			}
			if (!is_match) {
				continue;
			}
			shifts[i * 255 + current_char] = main_idx + 1;
		}
	}
}

void BufferedCSVReader::Initialize(const vector<LogicalType> &requested_types) {
	PrepareComplexParser();
	if (options.auto_detect) {
		return_types = SniffCSV(requested_types);
		if (return_types.empty()) {
			throw InvalidInputException("Failed to detect column types from CSV: is the file a valid CSV file?");
		}
		JumpToBeginning(options.skip_rows, options.header);
	} else {
		return_types = requested_types;
		ResetBuffer();
		SkipRowsAndReadHeader(options.skip_rows, options.header);
	}
	InitParseChunk(return_types.size());
}

void BufferedCSVReader::ResetBuffer() {
	buffer.reset();
	buffer_size = 0;
	position = 0;
	start = 0;
	cached_buffers.clear();
}

void BufferedCSVReader::ResetStream() {
	file_handle->Reset();
	linenr = 0;
	linenr_estimated = false;
	bytes_per_line_avg = 0;
	sample_chunk_idx = 0;
	jumping_samples = false;
}

void BufferedCSVReader::JumpToBeginning(idx_t skip_rows = 0, bool skip_header = false) {
	ResetBuffer();
	ResetStream();
	sample_chunk_idx = 0;
	bytes_in_chunk = 0;
	end_of_file_reached = false;
	bom_checked = false;
	SkipRowsAndReadHeader(skip_rows, skip_header);
}

void BufferedCSVReader::SkipRowsAndReadHeader(idx_t skip_rows, bool skip_header) {
	for (idx_t i = 0; i < skip_rows; i++) {
		// ignore skip rows
		string read_line = file_handle->ReadLine();
		linenr++;
	}

	if (skip_header) {
		// ignore the first line as a header line
		InitParseChunk(return_types.size());
		ParseCSV(ParserMode::PARSING_HEADER);
	}
}

void BufferedCSVReader::PrepareComplexParser() {
	delimiter_search = TextSearchShiftArray(options.delimiter);
	escape_search = TextSearchShiftArray(options.escape);
	quote_search = TextSearchShiftArray(options.quote);
}

string BufferedCSVReader::ColumnTypesError(case_insensitive_map_t<idx_t> sql_types_per_column,
                                           const vector<string> &names) {
	for (idx_t i = 0; i < names.size(); i++) {
		auto it = sql_types_per_column.find(names[i]);
		if (it != sql_types_per_column.end()) {
			sql_types_per_column.erase(names[i]);
			continue;
		}
	}
	if (sql_types_per_column.empty()) {
		return string();
	}
	string exception = "COLUMN_TYPES error: Columns with names: ";
	for (auto &col : sql_types_per_column) {
		exception += "\"" + col.first + "\",";
	}
	exception.pop_back();
	exception += " do not exist in the CSV File";
	return exception;
}

bool BufferedCSVReader::TryParseComplexCSV(DataChunk &insert_chunk, string &error_message) {
	// used for parsing algorithm
	bool finished_chunk = false;
	idx_t column = 0;
	vector<idx_t> escape_positions;
	bool has_quotes = false;
	uint8_t delimiter_pos = 0, escape_pos = 0, quote_pos = 0;
	idx_t offset = 0;
	idx_t line_start = 0;
	// read values into the buffer (if any)
	if (position >= buffer_size) {
		if (!ReadBuffer(start, line_start)) {
			return true;
		}
	}
	// start parsing the first value
	start = position;
	goto value_start;
value_start:
	/* state: value_start */
	// this state parses the first characters of a value
	offset = 0;
	delimiter_pos = 0;
	quote_pos = 0;
	do {
		idx_t count = 0;
		for (; position < buffer_size; position++) {
			quote_search.Match(quote_pos, buffer[position]);
			delimiter_search.Match(delimiter_pos, buffer[position]);
			count++;
			if (delimiter_pos == options.delimiter.size()) {
				// found a delimiter, add the value
				offset = options.delimiter.size() - 1;
				goto add_value;
			} else if (StringUtil::CharacterIsNewline(buffer[position])) {
				// found a newline, add the row
				goto add_row;
			}
			if (count > quote_pos) {
				// did not find a quote directly at the start of the value, stop looking for the quote now
				goto normal;
			}
			if (quote_pos == options.quote.size()) {
				// found a quote, go to quoted loop and skip the initial quote
				start += options.quote.size();
				goto in_quotes;
			}
		}
	} while (ReadBuffer(start, line_start));
	// file ends while scanning for quote/delimiter, go to final state
	goto final_state;
normal:
	/* state: normal parsing state */
	// this state parses the remainder of a non-quoted value until we reach a delimiter or newline
	position++;
	do {
		for (; position < buffer_size; position++) {
			delimiter_search.Match(delimiter_pos, buffer[position]);
			if (delimiter_pos == options.delimiter.size()) {
				offset = options.delimiter.size() - 1;
				goto add_value;
			} else if (StringUtil::CharacterIsNewline(buffer[position])) {
				goto add_row;
			}
		}
	} while (ReadBuffer(start, line_start));
	goto final_state;
add_value:
	AddValue(string_t(buffer.get() + start, position - start - offset), column, escape_positions, has_quotes);
	// increase position by 1 and move start to the new position
	offset = 0;
	has_quotes = false;
	start = ++position;
	if (position >= buffer_size && !ReadBuffer(start, line_start)) {
		// file ends right after delimiter, go to final state
		goto final_state;
	}
	goto value_start;
add_row : {
	// check type of newline (\r or \n)
	bool carriage_return = buffer[position] == '\r';
	AddValue(string_t(buffer.get() + start, position - start - offset), column, escape_positions, has_quotes);
	finished_chunk = AddRow(insert_chunk, column, error_message);

	if (!error_message.empty()) {
		return false;
	}
	// increase position by 1 and move start to the new position
	offset = 0;
	has_quotes = false;
	position++;
	SkipEmptyLines();
	start = position;
	if (position >= buffer_size && !ReadBuffer(start, line_start)) {
		// file ends right after newline, go to final state
		goto final_state;
	}
	if (carriage_return) {
		// \r newline, go to special state that parses an optional \n afterwards
		goto carriage_return;
	} else {
		// \n newline, move to value start
		if (finished_chunk) {
			return true;
		}
		goto value_start;
	}
}
in_quotes:
	/* state: in_quotes */
	// this state parses the remainder of a quoted value
	quote_pos = 0;
	escape_pos = 0;
	has_quotes = true;
	position++;
	do {
		for (; position < buffer_size; position++) {
			quote_search.Match(quote_pos, buffer[position]);
			escape_search.Match(escape_pos, buffer[position]);
			if (quote_pos == options.quote.size()) {
				goto unquote;
			} else if (escape_pos == options.escape.size()) {
				escape_positions.push_back(position - start - (options.escape.size() - 1));
				goto handle_escape;
			}
		}
	} while (ReadBuffer(start, line_start));
	// still in quoted state at the end of the file, error:
	error_message = StringUtil::Format("Error in file \"%s\" on line %s: unterminated quotes. (%s)", options.file_path,
	                                   GetLineNumberStr(linenr, linenr_estimated).c_str(), options.ToString());
	return false;
unquote:
	/* state: unquote */
	// this state handles the state directly after we unquote
	// in this state we expect either another quote (entering the quoted state again, and escaping the quote)
	// or a delimiter/newline, ending the current value and moving on to the next value
	delimiter_pos = 0;
	quote_pos = 0;
	position++;
	if (position >= buffer_size && !ReadBuffer(start, line_start)) {
		// file ends right after unquote, go to final state
		offset = options.quote.size();
		goto final_state;
	}
	if (StringUtil::CharacterIsNewline(buffer[position])) {
		// quote followed by newline, add row
		offset = options.quote.size();
		goto add_row;
	}
	do {
		idx_t count = 0;
		for (; position < buffer_size; position++) {
			quote_search.Match(quote_pos, buffer[position]);
			delimiter_search.Match(delimiter_pos, buffer[position]);
			count++;
			if (count > delimiter_pos && count > quote_pos) {
				error_message = StringUtil::Format(
				    "Error in file \"%s\" on line %s: quote should be followed by end of value, end "
				    "of row or another quote. (%s)",
				    options.file_path, GetLineNumberStr(linenr, linenr_estimated).c_str(), options.ToString());
				return false;
			}
			if (delimiter_pos == options.delimiter.size()) {
				// quote followed by delimiter, add value
				offset = options.quote.size() + options.delimiter.size() - 1;
				goto add_value;
			} else if (quote_pos == options.quote.size() &&
			           (options.escape.empty() || options.escape == options.quote)) {
				// quote followed by quote, go back to quoted state and add to escape
				escape_positions.push_back(position - start - (options.quote.size() - 1));
				goto in_quotes;
			}
		}
	} while (ReadBuffer(start, line_start));
	error_message = StringUtil::Format(
	    "Error in file \"%s\" on line %s: quote should be followed by end of value, end of row or another quote. (%s)",
	    options.file_path, GetLineNumberStr(linenr, linenr_estimated).c_str(), options.ToString());
	return false;
handle_escape:
	escape_pos = 0;
	quote_pos = 0;
	position++;
	do {
		idx_t count = 0;
		for (; position < buffer_size; position++) {
			quote_search.Match(quote_pos, buffer[position]);
			escape_search.Match(escape_pos, buffer[position]);
			count++;
			if (count > escape_pos && count > quote_pos) {
				error_message = StringUtil::Format(
				    "Error in file \"%s\" on line %s: neither QUOTE nor ESCAPE is proceeded by ESCAPE. (%s)",
				    options.file_path, GetLineNumberStr(linenr, linenr_estimated).c_str(), options.ToString());
				return false;
			}
			if (quote_pos == options.quote.size() || escape_pos == options.escape.size()) {
				// found quote or escape: move back to quoted state
				goto in_quotes;
			}
		}
	} while (ReadBuffer(start, line_start));
	error_message =
	    StringUtil::Format("Error in file \"%s\" on line %s: neither QUOTE nor ESCAPE is proceeded by ESCAPE. (%s)",
	                       options.file_path, GetLineNumberStr(linenr, linenr_estimated).c_str(), options.ToString());
	return false;
carriage_return:
	/* state: carriage_return */
	// this stage optionally skips a newline (\n) character, which allows \r\n to be interpreted as a single line
	if (buffer[position] == '\n') {
		// newline after carriage return: skip
		start = ++position;
		if (position >= buffer_size && !ReadBuffer(start, line_start)) {
			// file ends right after newline, go to final state
			goto final_state;
		}
	}
	if (finished_chunk) {
		return true;
	}
	goto value_start;
final_state:
	if (finished_chunk) {
		return true;
	}
	if (column > 0 || position > start) {
		// remaining values to be added to the chunk
		AddValue(string_t(buffer.get() + start, position - start - offset), column, escape_positions, has_quotes);
		finished_chunk = AddRow(insert_chunk, column, error_message);
		SkipEmptyLines();
		if (!error_message.empty()) {
			return false;
		}
	}
	// final stage, only reached after parsing the file is finished
	// flush the parsed chunk and finalize parsing
	if (mode == ParserMode::PARSING) {
		Flush(insert_chunk);
	}

	end_of_file_reached = true;
	return true;
}

void BufferedCSVReader::SkipEmptyLines() {
	if (parse_chunk.data.size() == 1) {
		// Empty lines are null data.
		return;
	}
	for (; position < buffer_size; position++) {
		if (!StringUtil::CharacterIsNewline(buffer[position])) {
			return;
		}
	}
}

void UpdateMaxLineLength(ClientContext &context, idx_t line_length) {
	if (!context.client_data->debug_set_max_line_length) {
		return;
	}
	if (line_length < context.client_data->debug_max_line_length) {
		return;
	}
	context.client_data->debug_max_line_length = line_length;
}

bool BufferedCSVReader::TryParseSimpleCSV(DataChunk &insert_chunk, string &error_message) {
	// used for parsing algorithm
	bool finished_chunk = false;
	idx_t column = 0;
	idx_t offset = 0;
	bool has_quotes = false;
	vector<idx_t> escape_positions;

	idx_t line_start = position;
	// read values into the buffer (if any)
	if (position >= buffer_size) {
		if (!ReadBuffer(start, line_start)) {
			return true;
		}
	}

	// start parsing the first value
	goto value_start;
value_start:
	offset = 0;
	/* state: value_start */
	// this state parses the first character of a value
	if (buffer[position] == options.quote[0]) {
		// quote: actual value starts in the next position
		// move to in_quotes state
		start = position + 1;
		goto in_quotes;
	} else {
		// no quote, move to normal parsing state
		start = position;
		goto normal;
	}
normal:
	/* state: normal parsing state */
	// this state parses the remainder of a non-quoted value until we reach a delimiter or newline
	do {
		for (; position < buffer_size; position++) {
			if (buffer[position] == options.delimiter[0]) {
				// delimiter: end the value and add it to the chunk
				goto add_value;
			} else if (StringUtil::CharacterIsNewline(buffer[position])) {
				// newline: add row
				goto add_row;
			}
		}
	} while (ReadBuffer(start, line_start));
	// file ends during normal scan: go to end state
	goto final_state;
add_value:
	AddValue(string_t(buffer.get() + start, position - start - offset), column, escape_positions, has_quotes);
	// increase position by 1 and move start to the new position
	offset = 0;
	has_quotes = false;
	start = ++position;
	if (position >= buffer_size && !ReadBuffer(start, line_start)) {
		// file ends right after delimiter, go to final state
		goto final_state;
	}
	goto value_start;
add_row : {
	// check type of newline (\r or \n)
	bool carriage_return = buffer[position] == '\r';
	AddValue(string_t(buffer.get() + start, position - start - offset), column, escape_positions, has_quotes);
	if (!error_message.empty()) {
		return false;
	}
	finished_chunk = AddRow(insert_chunk, column, error_message);
	UpdateMaxLineLength(context, position - line_start);
	if (!error_message.empty()) {
		return false;
	}
	// increase position by 1 and move start to the new position
	offset = 0;
	has_quotes = false;
	position++;
	start = position;
	line_start = position;
	if (position >= buffer_size && !ReadBuffer(start, line_start)) {
		// file ends right after delimiter, go to final state
		goto final_state;
	}
	if (carriage_return) {
		// \r newline, go to special state that parses an optional \n afterwards
		goto carriage_return;
	} else {
		SetNewLineDelimiter();
		SkipEmptyLines();

		start = position;
		line_start = position;
		if (position >= buffer_size && !ReadBuffer(start, line_start)) {
			// file ends right after delimiter, go to final state
			goto final_state;
		}
		// \n newline, move to value start
		if (finished_chunk) {
			return true;
		}
		goto value_start;
	}
}
in_quotes:
	/* state: in_quotes */
	// this state parses the remainder of a quoted value
	has_quotes = true;
	position++;
	do {
		for (; position < buffer_size; position++) {
			if (buffer[position] == options.quote[0]) {
				// quote: move to unquoted state
				goto unquote;
			} else if (buffer[position] == options.escape[0]) {
				// escape: store the escaped position and move to handle_escape state
				escape_positions.push_back(position - start);
				goto handle_escape;
			}
		}
	} while (ReadBuffer(start, line_start));
	// still in quoted state at the end of the file, error:
	throw InvalidInputException("Error in file \"%s\" on line %s: unterminated quotes. (%s)", options.file_path,
	                            GetLineNumberStr(linenr, linenr_estimated).c_str(), options.ToString());
unquote:
	/* state: unquote */
	// this state handles the state directly after we unquote
	// in this state we expect either another quote (entering the quoted state again, and escaping the quote)
	// or a delimiter/newline, ending the current value and moving on to the next value
	position++;
	if (position >= buffer_size && !ReadBuffer(start, line_start)) {
		// file ends right after unquote, go to final state
		offset = 1;
		goto final_state;
	}
	if (buffer[position] == options.quote[0] && (options.escape.empty() || options.escape[0] == options.quote[0])) {
		// escaped quote, return to quoted state and store escape position
		escape_positions.push_back(position - start);
		goto in_quotes;
	} else if (buffer[position] == options.delimiter[0]) {
		// delimiter, add value
		offset = 1;
		goto add_value;
	} else if (StringUtil::CharacterIsNewline(buffer[position])) {
		offset = 1;
		goto add_row;
	} else {
		error_message = StringUtil::Format(
		    "Error in file \"%s\" on line %s: quote should be followed by end of value, end of "
		    "row or another quote. (%s)",
		    options.file_path, GetLineNumberStr(linenr, linenr_estimated).c_str(), options.ToString());
		return false;
	}
handle_escape:
	/* state: handle_escape */
	// escape should be followed by a quote or another escape character
	position++;
	if (position >= buffer_size && !ReadBuffer(start, line_start)) {
		error_message = StringUtil::Format(
		    "Error in file \"%s\" on line %s: neither QUOTE nor ESCAPE is proceeded by ESCAPE. (%s)", options.file_path,
		    GetLineNumberStr(linenr, linenr_estimated).c_str(), options.ToString());
		return false;
	}
	if (buffer[position] != options.quote[0] && buffer[position] != options.escape[0]) {
		error_message = StringUtil::Format(
		    "Error in file \"%s\" on line %s: neither QUOTE nor ESCAPE is proceeded by ESCAPE. (%s)", options.file_path,
		    GetLineNumberStr(linenr, linenr_estimated).c_str(), options.ToString());
		return false;
	}
	// escape was followed by quote or escape, go back to quoted state
	goto in_quotes;
carriage_return:
	/* state: carriage_return */
	// this stage optionally skips a newline (\n) character, which allows \r\n to be interpreted as a single line
	if (buffer[position] == '\n') {
		SetNewLineDelimiter(true, true);
		// newline after carriage return: skip
		// increase position by 1 and move start to the new position
		start = ++position;
		if (position >= buffer_size && !ReadBuffer(start, line_start)) {
			// file ends right after delimiter, go to final state
			goto final_state;
		}
	} else {
		SetNewLineDelimiter(true, false);
	}
	if (finished_chunk) {
		return true;
	}
	SkipEmptyLines();
	start = position;
	line_start = position;
	if (position >= buffer_size && !ReadBuffer(start, line_start)) {
		// file ends right after delimiter, go to final state
		goto final_state;
	}

	goto value_start;
final_state:
	if (finished_chunk) {
		return true;
	}

	if (column > 0 || position > start) {
		// remaining values to be added to the chunk
		AddValue(string_t(buffer.get() + start, position - start - offset), column, escape_positions, has_quotes);
		finished_chunk = AddRow(insert_chunk, column, error_message);
		SkipEmptyLines();
		UpdateMaxLineLength(context, position - line_start);
		if (!error_message.empty()) {
			return false;
		}
	}

	// final stage, only reached after parsing the file is finished
	// flush the parsed chunk and finalize parsing
	if (mode == ParserMode::PARSING) {
		Flush(insert_chunk);
	}

	end_of_file_reached = true;
	return true;
}

bool BufferedCSVReader::ReadBuffer(idx_t &start, idx_t &line_start) {
	if (start > buffer_size) {
		return false;
	}
	auto old_buffer = std::move(buffer);

	// the remaining part of the last buffer
	idx_t remaining = buffer_size - start;

	bool large_buffers = mode == ParserMode::PARSING && !file_handle->OnDiskFile() && file_handle->CanSeek();
	idx_t buffer_read_size = large_buffers ? INITIAL_BUFFER_SIZE_LARGE : INITIAL_BUFFER_SIZE;

	while (remaining > buffer_read_size) {
		buffer_read_size *= 2;
	}

	// Check line length
	if (remaining > options.maximum_line_size) {
		throw InvalidInputException("Maximum line size of %llu bytes exceeded on line %s!", options.maximum_line_size,
		                            GetLineNumberStr(linenr, linenr_estimated));
	}

	buffer = make_unsafe_uniq_array<char>(buffer_read_size + remaining + 1);
	buffer_size = remaining + buffer_read_size;
	if (remaining > 0) {
		// remaining from last buffer: copy it here
		memcpy(buffer.get(), old_buffer.get() + start, remaining);
	}
	idx_t read_count = file_handle->Read(buffer.get() + remaining, buffer_read_size);

	bytes_in_chunk += read_count;
	buffer_size = remaining + read_count;
	buffer[buffer_size] = '\0';
	if (old_buffer) {
		cached_buffers.push_back(std::move(old_buffer));
	}
	start = 0;
	position = remaining;
	if (!bom_checked) {
		bom_checked = true;
		if (read_count >= 3 && buffer[0] == '\xEF' && buffer[1] == '\xBB' && buffer[2] == '\xBF') {
			start += 3;
			position += 3;
		}
	}
	line_start = start;

	return read_count > 0;
}

void BufferedCSVReader::ParseCSV(DataChunk &insert_chunk) {
	string error_message;
	if (!TryParseCSV(ParserMode::PARSING, insert_chunk, error_message)) {
		throw InvalidInputException(error_message);
	}
}

bool BufferedCSVReader::TryParseCSV(ParserMode mode) {
	DataChunk dummy_chunk;
	string error_message;
	return TryParseCSV(mode, dummy_chunk, error_message);
}

void BufferedCSVReader::ParseCSV(ParserMode mode) {
	DataChunk dummy_chunk;
	string error_message;
	if (!TryParseCSV(mode, dummy_chunk, error_message)) {
		throw InvalidInputException(error_message);
	}
}

bool BufferedCSVReader::TryParseCSV(ParserMode parser_mode, DataChunk &insert_chunk, string &error_message) {
	mode = parser_mode;

	if (options.quote.size() <= 1 && options.escape.size() <= 1 && options.delimiter.size() == 1) {
		return TryParseSimpleCSV(insert_chunk, error_message);
	} else {
		return TryParseComplexCSV(insert_chunk, error_message);
	}
}

} // namespace duckdb
