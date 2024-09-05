import pickle

import duckdb
import numpy as np
from duckdb.typing import BIGINT, DOUBLE, FLOAT, VARCHAR
import pandas as pd
import time
from sklearn.preprocessing import OneHotEncoder
from sklearn.preprocessing import StandardScaler
from tqdm import tqdm
# hand_type = "udf"
hand_type = "special"
name = "pf3"


con = duckdb.connect(
    "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_raven.db")

root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"


scaler_path = f'{root_model_path}/Flights/flights_standard_scale_model.pkl'
enc_path = f'{root_model_path}/Flights/flights_one_hot_encoder.pkl'
model_path = f'{root_model_path}/Flights/flights_rf_model.pkl'
with open(scaler_path, 'rb') as f:
    scaler = pickle.load(f)
with open(enc_path, 'rb') as f:
    enc = pickle.load(f)
with open(model_path, 'rb') as f:
    model = pickle.load(f)


def udf(slatitude, slongitude, dlatitude, dlongitude, name1, name2, name4, acountry, active,
        scity, scountry, stimezone, sdst, dcity, dcountry, dtimezone, ddst):
    data = np.column_stack([slatitude, slongitude, dlatitude, dlongitude, name1, name2, name4, acountry, active,
                            scity, scountry, stimezone, sdst, dcity, dcountry, dtimezone, ddst])
    data = np.split(data, np.array([4]), axis=1)
    numerical = data[0]
    categorical = data[1]

    X = np.hstack((scaler.transform(numerical),
                  enc.transform(categorical).toarray()))
    return model.predict(X)


sql = '''
SELECT slatitude, slongitude, dlatitude, dlongitude, name1, name2, name4, acountry, active, 
        scity, scountry, stimezone, sdst, dcity, dcountry, dtimezone, ddst
FROM Flights_S_routes_extension JOIN Flights_R1_airlines ON Flights_S_routes_extension.airlineid = Flights_R1_airlines.airlineid 
JOIN Flights_R2_sairports ON Flights_S_routes_extension.sairportid = Flights_R2_sairports.sairportid JOIN Flights_R3_dairports 
ON Flights_S_routes_extension.dairportid = Flights_R3_dairports.dairportid
where slatitude > 26 and dlatitude > 30 and slatitude < 40 and dlatitude < 40
'''
times = 5
min1 = 0
max1 = 0
res = 0
flag = True
for i in tqdm(range(times)):
    s = time.time()
    res_data = con.sql(sql).fetch_arrow_table()
    udf(*res_data)
    e = time.time()
    t = e-s
    res = res + t
    if flag:
        flag = False
        min1 = t
        max1 = t
    else:
        min1 = t if min1 > t else min1
        max1 = t if max1 < t else max1

res = res - min1 - max1
times = times - 2
print(f"{name}, {res/times}s ")
