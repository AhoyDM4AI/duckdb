#!/bin/bash
eval "$(conda shell.bash hook)"
conda activate duckdb
/root/workspace/duckdb/examples/embedded-c++/build/udf_server $1