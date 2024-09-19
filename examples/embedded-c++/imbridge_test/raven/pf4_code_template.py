import pyarrow as pa
import numpy as np
import pandas as pd
import onnxruntime as ort


class MyProcess:
    def __init__(self):
        # load model part
        self.root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"
        self.onnx_path = f'{self.root_model_path}/Flights/flights_rf_pipeline.onnx'
        ortconfig = ort.SessionOptions()
        self.flights_onnx_session = ort.InferenceSession(
            self.onnx_path, sess_options=ortconfig)
        self.expedia_label = self.flights_onnx_session.get_outputs()[0]
        numerical_columns = ['slatitude',
                             'slongitude', 'dlatitude', 'dlongitude']
        categorical_columns = ['name1', 'name2', 'name4', 'acountry', 'active', 'scity', 'scountry', 'stimezone', 'sdst',
                               'dcity', 'dcountry', 'dtimezone', 'ddst']
        self.flights_input_columns = numerical_columns + categorical_columns
        self.flights_type_map = {
            'int32': np.int64,
            'int64': np.int64,
            'float64': np.float32,
            'object': str,
        }

    def process(self, table):
        def udf_wrap(*args):
            infer_batch = {
                elem: args[i].to_numpy().astype(self.flights_type_map[args[i].to_numpy().dtype.name]).reshape((-1, 1))
                for i, elem in enumerate(self.flights_input_columns)
            }
            outputs = self.flights_onnx_session.run([self.flights_label.name], infer_batch)
            return outputs[0]
        res = udf_wrap(*table)
        df = pd.DataFrame(res)
        return pa.Table.from_pandas(df)
