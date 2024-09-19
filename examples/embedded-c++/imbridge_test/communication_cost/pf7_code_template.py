import pyarrow as pa
import numpy as np
import pandas as pd


    
class MyProcess:
    def __init__(self):
        # load model part
        pass


    def process(self, table):
        # print(table.num_rows)
        data = table.to_pandas().values[:, 0]
        data = pd.DataFrame(data)
        return pa.Table.from_pandas(data)