
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
	con.CreateVectorizedFunction<int64_t, double, double>("udf", &udf_tmp, LogicalType::INVALID,
	                                                      FunctionKind::PREDICTION, 4096);

	string sql = R"(
explain analyze select transactionID, udf(amount_norm, business_hour_norm) 
from (select transactionID, amount/transaction_limit amount_norm, hour(strptime(time, '%Y-%m-%dT%H:%M'))/23 business_hour_norm 
from Financial_Account join Financial_Transactions on Financial_Account.fa_customer_sk = Financial_Transactions.senderID);
)";
	int times = 5;
	double result = 0;
	double min1, max1;
	bool flag = true;
	for (int i = 0; i < times; i++) {
		clock_t start_time = clock();
		con.Query(sql);
		clock_t end_time = clock();
		double t = (double)(end_time - start_time) / CLOCKS_PER_SEC;
		result += t;
		if (flag) {
			flag = false;
			min1 = t;
			max1 = t;
		} else {
			min1 = std::min(min1, t);
			max1 = std::max(max1, t);
		}
	}
	result = result - min1 - max1;
	times = times - 2;
	printf("finished execute %lf s!\n", result / (times * 1.0));
	return 0;
}