import pyarrow as pa
import numpy as np
import pandas as pd
import onnxruntime as ort


class MyProcess:
    def __init__(self):
        # load model part
        self.root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"
        self.onnx_path = f'{self.root_model_path}/Hospital/hospital_mlp_pipeline.onnx'
        ortconfig = ort.SessionOptions()
        self.hospital_onnx_session = ort.InferenceSession(
            self.onnx_path, sess_options=ortconfig)
        self.hospital_label = self.hospital_onnx_session.get_outputs()[0]
        numerical_columns = ['hematocrit', 'neutrophils', 'sodium', 'glucose', 'bloodureanitro', 'creatinine', 'bmi', 'pulse',
                             'respiration', 'secondarydiagnosisnonicd9']
        categorical_columns = ['rcount', 'gender', 'dialysisrenalendstage', 'asthma', 'irondef', 'pneum', 'substancedependence',
                               'psychologicaldisordermajor', 'depress', 'psychother', 'fibrosisandother', 'malnutrition',
                               'hemo']
        self.hospital_input_columns = numerical_columns + categorical_columns
        self.hospital_type_map = {
            'int32': np.int64,
            'int64': np.int64,
            'float64': np.float32,
            'object': str,
        }

    def process(self, table):
        def udf_wrap(*args):
            infer_batch = {
                elem: args[i].to_numpy().astype(self.hospital_type_map[args[i].to_numpy().dtype.name]).reshape((-1, 1))
                for i, elem in enumerate(self.hospital_input_columns)
            }
            outputs = self.hospital_onnx_session.run([self.hospital_label.name], infer_batch)
            df = pd.DataFrame(outputs[0])
            return pa.Table.from_pandas(df)
        return udf_wrap(*table)
