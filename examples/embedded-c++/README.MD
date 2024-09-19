# UDF_Server 



## file structure


```
./
├── CMakeLists.txt
├── code.py                             # use for execution
├── code_template.py                    # execute python code template
├── imbridge_test
│   ├── CMakeLists.txt
│   ├── data ->                         # link to your data path
│   ├── db                              # duckdb storage file
│   │   ├── db_raven.db
│   │   └── db_tpcx_ai_sf10.db
│   ├── raven
│   │   ├── CMakeLists.txt
│   │   ├── load_data_pf.py             # load raven datasets
│   │   ├── pf1_code_template.py        # execute python code template for pf1
│   │   ├── pf1.cpp                     # execution program
│   │   └── pf1.py                      # origin python execution
│   │   └── ...
│   └── tpcx_ai
│       ├── CMakeLists.txt
│       └── load_data_tpc_ai.py         # load tpcx_ai datasets
│       └── ...                         # the same as raven
├── main.cpp
├── Makefile
├── README.MD
├── server_start.sh                     # start the server in client
├── test_prediction_native.cpp
├── udf.cpp 
└── udf_server.cpp                      # udf_server core 
```
