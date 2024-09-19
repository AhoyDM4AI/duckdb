import pyarrow as pa
import numpy as np
import pandas as pd
import onnxruntime as ort

    

class MyProcess:
    def __init__(self):
        # load model part
        self.root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"
        self.onnx_path = f'{self.root_model_path}/Expedia/expedia_dt_pipeline.onnx'
        ortconfig = ort.SessionOptions()
        self.expedia_onnx_session = ort.InferenceSession(self.onnx_path, sess_options=ortconfig)
        self.expedia_label = self.expedia_onnx_session.get_outputs()[0]
        numerical_columns = ['prop_location_score1', 'prop_location_score2', 'prop_log_historical_price', 'price_usd', 'orig_destination_distance', 'prop_review_score', 'avg_bookings_usd', 'stdev_bookings_usd']
        categorical_columns = ['position', 'prop_country_id', 'prop_starrating', 'prop_brand_bool', 'count_clicks','count_bookings', 'year', 'month', 'weekofyear', 'time', 'site_id', 'visitor_location_country_id', 'srch_destination_id', 'srch_length_of_stay', 'srch_booking_window', 'srch_adults_count', 'srch_children_count', 'srch_room_count', 'srch_saturday_night_bool', 'random_bool']
        self.expedia_input_columns = numerical_columns + categorical_columns
        self.expedia_type_map = {
            'bool': np.int64,
            'int32': np.int64,
            'int64': np.int64,
            'float64': np.float32,
            'object': str,
        }


    def process(self, table):
        def udf_wrap(*args):
            infer_batch = {
                elem: args[i].astype(self.expedia_type_map[args[i].dtype.name]).reshape((-1, 1))
                for i, elem in enumerate(self.expedia_input_columns)
            }
            outputs = self.expedia_onnx_session.run([self.expedia_label.name], infer_batch)
            return outputs[0]
        res = udf_wrap(*table)
        df = pd.DataFrame(res)
        return pa.Table.from_pandas(df)