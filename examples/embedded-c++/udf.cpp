
#include "duckdb.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace duckdb;
using namespace imbridge;

static void udf_tmp(DataChunk &input, ExpressionState &state, Vector &result) {
	using TYPE = double;
	result.SetVectorType(VectorType::FLAT_VECTOR);
	auto result_data = ConstantVector::GetData<TYPE>(result);
	input.Flatten();
	auto tmp_data1 = ConstantVector::GetData<TYPE>(input.data[0]);
	auto tmp_data2 = ConstantVector::GetData<TYPE>(input.data[1]);
	memset(result_data, std::numeric_limits<int64_t>::min(), input.size() * sizeof(int64_t));
	for (idx_t i = 0; i < input.size(); i++) {
		result_data[i] = (int)tmp_data1[i];
	}
}

int main() {
	DuckDB db("/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_tpcx_ai_sf10.db");
	Connection con(db);
	con.CreateVectorizedFunction<
	    int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
	    int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
	    int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
	    int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
	    int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
	    int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
	    int64_t, int64_t, int64_t, int64_t, int64_t, int64_t>("udf", &udf_tmp, LogicalType::INVALID,
	                                                          FunctionKind::PREDICTION, 4096);
	string sql = R"(
explain analyze select o_order_id, udf(cast(scan_count as INTEGER), cast(scan_count_abs as INTEGER), 
Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday, cast(dep0 as INTEGER), cast(dep1 as INTEGER),
 cast(dep2 as INTEGER), cast(dep3 as INTEGER), cast(dep4 as INTEGER), cast(dep5 as INTEGER), cast(dep6 as INTEGER),
  cast(dep7 as INTEGER), cast(dep8 as INTEGER), cast(dep9 as INTEGER), cast(dep10 as INTEGER), cast(dep11 as INTEGER),
   cast(dep12 as INTEGER), cast(dep13 as INTEGER), cast(dep14 as INTEGER), cast(dep15 as INTEGER), cast(dep16 as INTEGER),
    cast(dep17 as INTEGER), cast(dep18 as INTEGER), cast(dep19 as INTEGER), cast(dep20 as INTEGER), cast(dep21 as INTEGER),
     cast(dep22 as INTEGER), cast(dep23 as INTEGER), cast(dep24 as INTEGER), cast(dep25 as INTEGER), cast(dep26 as INTEGER),
      cast(dep27 as INTEGER), cast(dep28 as INTEGER), cast(dep29 as INTEGER), cast(dep30 as INTEGER), cast(dep31 as INTEGER), 
      cast(dep32 as INTEGER), cast(dep33 as INTEGER), cast(dep34 as INTEGER), cast(dep35 as INTEGER), cast(dep36 as INTEGER), 
      cast(dep37 as INTEGER), cast(dep38 as INTEGER), cast(dep39 as INTEGER), cast(dep40 as INTEGER), cast(dep41 as INTEGER), 
      cast(dep42 as INTEGER), cast(dep43 as INTEGER), cast(dep44 as INTEGER), cast(dep45 as INTEGER), cast(dep46 as INTEGER),
       cast(dep47 as INTEGER), cast(dep48 as INTEGER), cast(dep49 as INTEGER), cast(dep50 as INTEGER), cast(dep51 as INTEGER),
        cast(dep52 as INTEGER), cast(dep53 as INTEGER), cast(dep54 as INTEGER), cast(dep55 as INTEGER), cast(dep56 as INTEGER),
         cast(dep57 as INTEGER), cast(dep58 as INTEGER), cast(dep59 as INTEGER), cast(dep60 as INTEGER), cast(dep61 as INTEGER),
          cast(dep62 as INTEGER), cast(dep63 as INTEGER), cast(dep64 as INTEGER), cast(dep65 as INTEGER), cast(dep66 as INTEGER),
           cast(dep67 as INTEGER)) 
from uc08_used_data;
)";
con.Query("SET threads TO 1;");
con.Query(sql)->Print();
	return 0;
}