import joblib
import numpy as np
import pyarrow as pa
import pandas as pd
import joblib
from surprise import SVD
from surprise import Dataset
from surprise.reader import Reader


class MyProcess:
    def __init__(self):
        # load model part
        scale = 10
        name = "uc10"
        root_model_path = f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/tpcxai_datasets/sf{scale}"
        model_file_name = f"{root_model_path}/model/{name}/{name}.python.model"
        self.model = joblib.load(model_file_name)

    def process(self, table):
        def udf(business_hour_norm, amount_norm):
            data = pd.DataFrame({
                'business_hour_norm': business_hour_norm,
                'amount_norm': amount_norm
            })
            # print(data.shape)
            return self.model.predict(data)

        df = pd.DataFrame(udf(*table))
        return pa.Table.from_pandas(df)
