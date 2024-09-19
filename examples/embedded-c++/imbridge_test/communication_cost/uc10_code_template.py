import joblib
import numpy as np
import pyarrow as pa
import pandas as pd


class MyProcess:
    def __init__(self):
        # load model part
        pass

    def process(self, table):
        df = table.to_pandas().iloc[:, 0]
        res = pa.Table.from_pandas(pd.DataFrame(df))
        return res
