import joblib
import pyarrow as pa
import numpy as np
import pandas as pd
from imblearn.over_sampling import ADASYN
from imblearn.pipeline import Pipeline as Imb_Pipeline
from sklearn import metrics
from sklearn.metrics import classification_report, confusion_matrix, f1_score
from sklearn.preprocessing import StandardScaler
from sklearn.svm import SVC


class MyProcess:
    def __init__(self):
        # load model part
        self.scale = 10
        self.name = "uc06"
        self.root_model_path = f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/tpcxai_datasets/sf{self.scale}"
        self.model_file_name = f"{self.root_model_path}/model/{self.name}/{self.name}.python.model"
        self.model = joblib.load(self.model_file_name)


    def process(self, table):
        res = self.model.predict(table.to_pandas().values)
        df = pd.DataFrame(res)
        return pa.Table.from_pandas(df)
