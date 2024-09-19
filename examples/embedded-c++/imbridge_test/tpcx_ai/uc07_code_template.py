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
        name = "uc07"
        root_model_path = f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/tpcxai_datasets/sf{scale}"
        model_file_name = f"{root_model_path}/model/{name}/{name}.python.model"
        self.model = joblib.load(model_file_name)

    def process(self, table):
        def udf(user_id, item_id):
            ratings = []
            for i in range(len(user_id)):
                rating = self.model.predict(user_id[i], item_id[i]).est
                ratings.append(rating)
            return np.array(ratings)
        df = pd.DataFrame(udf(*table))
        return pa.Table.from_pandas(df)
