import pyarrow as pa

def process_table(table):
    col1 = table.column(0).to_pylist()
    col2 = table.column(1).to_pylist()
    result = [a + b for a, b in zip(col1, col2)]
    result_field = pa.field('result', pa.float64())
    result_schema = pa.schema([result_field])
    result_array = pa.array(result, type=pa.float64())
    result_table = pa.Table.from_arrays([result_array], schema=result_schema)
    return result_table

class MyProcess:
    def __init__(self):
        # load model part
        self.name = "MyTable"
        self.age = 18


    def process(self, table):
        # print("2333")
        print(self.name)
        print(self.age)
        print(table.num_rows)
        return process_table(table)