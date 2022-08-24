#include "duckdb/execution/index/art/node.hpp"
#include "duckdb/execution/index/art/leaf.hpp"
#include "duckdb/storage/meta_block_reader.hpp"
#include "duckdb/execution/index/art/prefix.hpp"
#include <cstring>

namespace duckdb {

Leaf::Leaf(Key &value, unsigned depth, row_t row_id) : Node(NodeType::NLeaf) {
	capacity = 1;
	row_ids = unique_ptr<row_t[]>(new row_t[capacity]);
	row_ids[0] = row_id;
	count = 1;
	D_ASSERT(value.len >= depth);
	prefix = Prefix(value, depth, value.len - depth);
}

Leaf::Leaf(Key &value, unsigned depth, unique_ptr<row_t[]> row_ids_p, idx_t num_elements_p) : Node(NodeType::NLeaf) {
	capacity = num_elements_p;
	row_ids = move(row_ids_p);
	count = num_elements_p;
	D_ASSERT(value.len >= depth);
	prefix = Prefix(value, depth, value.len - depth);
}

Leaf::Leaf(unique_ptr<row_t[]> row_ids_p, idx_t num_elements_p, Prefix &prefix_p) : Node(NodeType::NLeaf) {
	capacity = num_elements_p;
	row_ids = move(row_ids_p);
	count = num_elements_p;
	prefix = prefix_p;
}

void Leaf::Insert(row_t row_id) {

	// Grow array
	if (count == capacity) {
		auto new_row_id = unique_ptr<row_t[]>(new row_t[capacity * 2]);
		memcpy(new_row_id.get(), row_ids.get(), capacity * sizeof(row_t));
		capacity *= 2;
		row_ids = move(new_row_id);
	}
	row_ids[count++] = row_id;
}

BlockPointer Leaf::Serialize(duckdb::MetaBlockWriter &writer) {
	auto block_id = writer.block->id;
	uint32_t offset = writer.offset;
	// Write Node Type
	writer.Write(type);
	// Write compression Info
	prefix.Serialize(writer);
	// Write Row Ids
	// Length
	writer.Write(count);
	// Actual Row Ids
	for (idx_t i = 0; i < count; i++) {
		writer.Write(row_ids[i]);
	}
	return {block_id, offset};
}

Leaf *Leaf::Deserialize(MetaBlockReader &reader) {
	Prefix prefix;
	prefix.Deserialize(reader);
	auto num_elements = reader.Read<uint16_t>();
	auto elements = unique_ptr<row_t[]>(new row_t[num_elements]);
	for (idx_t i = 0; i < num_elements; i++) {
		elements[i] = reader.Read<row_t>();
	}
	return new Leaf(move(elements), num_elements, prefix);
}

void Leaf::Remove(row_t row_id) {
	idx_t entry_offset = DConstants::INVALID_INDEX;
	for (idx_t i = 0; i < count; i++) {
		if (row_ids[i] == row_id) {
			entry_offset = i;
			break;
		}
	}
	if (entry_offset == DConstants::INVALID_INDEX) {
		return;
	}
	count--;
	if (capacity > 2 && count < capacity / 2) {
		// Shrink array, if less than half full
		auto new_row_id = unique_ptr<row_t[]>(new row_t[capacity / 2]);
		memcpy(new_row_id.get(), row_ids.get(), entry_offset * sizeof(row_t));
		memcpy(new_row_id.get() + entry_offset, row_ids.get() + entry_offset + 1,
		       (count - entry_offset) * sizeof(row_t));
		capacity /= 2;
		row_ids = move(new_row_id);
	} else {
		// Copy the rest
		for (idx_t j = entry_offset; j < count; j++) {
			row_ids[j] = row_ids[j + 1];
		}
	}
}

void Leaf::Merge(ART *l_art, Node *&l_node, Node *&r_node) {

	Leaf *l_n = (Leaf *)l_node;
	Leaf *r_n = (Leaf *)r_node;

	// push row_ids of l_n in a map to avoid duplicates
	unordered_map<row_t, bool> l_n_row_ids;
	for (idx_t i = 0; i < l_n->count; i++) {
		l_n_row_ids[l_n->GetRowId(i)] = true;
	}

	// append non-duplicate row_ids to l_n
	for (idx_t i = 0; i < r_n->count; i++) {
		if (!l_n_row_ids[r_n->GetRowId(i)]) {
			l_n->Insert(r_n->GetRowId(i));
		}
	}

	if ((l_art->IsUnique() || l_art->IsPrimary()) && l_n->count > 1) {
		throw ConstraintException("Data contains duplicates on indexed column(s)");
	}
}

} // namespace duckdb
