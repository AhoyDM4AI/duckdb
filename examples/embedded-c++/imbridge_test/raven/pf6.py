import pickle

import duckdb
import numpy as np
from duckdb.typing import BIGINT, DOUBLE, FLOAT, VARCHAR
import pandas as pd
import time
import onnxruntime as ort
import time
from tqdm import tqdm
# hand_type = "udf"
hand_type = "special"
name = "pf6"

con = duckdb.connect(
    "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_raven.db")

root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"


onnx_path = f'{root_model_path}/Hospital/hospital_mlp_pipeline.onnx'
ortconfig = ort.SessionOptions()
hospital_onnx_session = ort.InferenceSession(onnx_path, sess_options=ortconfig)
hospital_label = hospital_onnx_session.get_outputs()[0]
numerical_columns = ['hematocrit', 'neutrophils', 'sodium', 'glucose', 'bloodureanitro', 'creatinine', 'bmi', 'pulse',
                     'respiration', 'secondarydiagnosisnonicd9']
categorical_columns = ['rcount', 'gender', 'dialysisrenalendstage', 'asthma', 'irondef', 'pneum', 'substancedependence',
                       'psychologicaldisordermajor', 'depress', 'psychother', 'fibrosisandother', 'malnutrition',
                       'hemo']
hospital_input_columns = numerical_columns + categorical_columns
hospital_type_map = {
    'int32': np.int64,
    'int64': np.int64,
    'float64': np.float32,
    'object': str,
}


def udf(hematocrit, neutrophils, sodium, glucose, bloodureanitro, creatinine, bmi, pulse, respiration,
        secondarydiagnosisnonicd9,
        rcount, gender, dialysisrenalendstage, asthma, irondef, pneum, substancedependence, psychologicaldisordermajor,
        depress, psychother, fibrosisandothe, malnutrition, hemo):
    def udf_wrap(*args):
        infer_batch = {
            elem: args[i].to_numpy().astype(hospital_type_map[args[i].to_numpy().dtype.name]).reshape((-1, 1))
            for i, elem in enumerate(hospital_input_columns)
        }
        outputs = hospital_onnx_session.run([hospital_label.name], infer_batch)
        return outputs[0]
    
    return udf_wrap(hematocrit, neutrophils, sodium, glucose, bloodureanitro, creatinine, bmi, pulse, respiration,
        secondarydiagnosisnonicd9,
        rcount, gender, dialysisrenalendstage, asthma, irondef, pneum, substancedependence, psychologicaldisordermajor,
        depress, psychother, fibrosisandothe, malnutrition, hemo)



con.create_function("udf", udf,
                    [DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, BIGINT,
                     DOUBLE, BIGINT, VARCHAR, VARCHAR,BIGINT, BIGINT,
                     BIGINT, BIGINT, BIGINT, BIGINT, BIGINT, BIGINT, BIGINT, BIGINT, BIGINT], BIGINT, type="arrow", null_handling=hand_type)
sql = '''
Explain analyze SELECT eid, udf(hematocrit, neutrophils, sodium, glucose, bloodureanitro, creatinine, bmi, pulse,
 respiration, secondarydiagnosisnonicd9, rcount, gender, cast(dialysisrenalendstage as INTEGER), cast(asthma as INTEGER),
  cast(irondef as INTEGER), cast(pneum as INTEGER), cast(substancedependence as INTEGER),
   cast(psychologicaldisordermajor as INTEGER), cast(depress as INTEGER), cast(psychother as INTEGER),
    cast(fibrosisandother as INTEGER), cast(malnutrition as INTEGER), cast(hemo as INTEGER)) AS lengthofstay
   FROM LengthOfStay_extension WHERE hematocrit > 10 AND neutrophils > 10 AND bloodureanitro < 20 AND pulse < 70;
'''
times = 5
min1 = 0
max1 = 0
res = 0
flag = True
for i in tqdm(range(times)):
    s = time.time()
    con.sql(sql)
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
