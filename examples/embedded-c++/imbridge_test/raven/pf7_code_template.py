import pickle
import pyarrow as pa
import numpy as np
import pandas as pd
from sklearn.preprocessing import OneHotEncoder
from sklearn.preprocessing import StandardScaler
import lightgbm as lgb

    
class MyProcess:
    def __init__(self):
        # load model part
        root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"
        self.scaler_path = f'{root_model_path}/Credit_Card/creditcard_standard_scale_model.pkl'
        self.model_path = f'{root_model_path}/Credit_Card/creditcard_lgb_model.txt'
        with open(self.scaler_path, 'rb') as f:
            self.scaler = pickle.load(f)
        self.model = lgb.Booster(model_file=self.model_path)


    def process(self, table):
        # print(table.num_rows)
        data = table.to_pandas().values
        numerical = np.column_stack(data.T)
        X = self.scaler.transform(numerical)
        res = self.model.predict(X)
        df = pd.DataFrame(res)
        return pa.Table.from_pandas(df)