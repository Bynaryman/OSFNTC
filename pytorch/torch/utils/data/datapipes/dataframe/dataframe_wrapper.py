try:
    import pandas  # type: ignore[import]

    # pandas used only for prototyping, will be shortly replaced with TorchArrow
    WITH_PANDAS = True
except ImportError:
    WITH_PANDAS = False


class PandasWrapper:
    @classmethod
    def create_dataframe(cls, data, columns):
        if not WITH_PANDAS:
            raise Exception("DataFrames prototype requires pandas to function")
        return pandas.DataFrame(data, columns=columns)

    @classmethod
    def is_dataframe(cls, data):
        if not WITH_PANDAS:
            return False
        return isinstance(data, pandas.core.frame.DataFrame)

    @classmethod
    def is_column(cls, data):
        if not WITH_PANDAS:
            return False
        return isinstance(data, pandas.core.series.Series)

    @classmethod
    def iterate(cls, data):
        if not WITH_PANDAS:
            raise Exception("DataFrames prototype requires pandas to function")
        for d in data:
            yield d

    @classmethod
    def concat(cls, buffer):
        if not WITH_PANDAS:
            raise Exception("DataFrames prototype requires pandas to function")
        return pandas.concat(buffer)

    @classmethod
    def get_item(cls, data, idx):
        if not WITH_PANDAS:
            raise Exception("DataFrames prototype requires pandas to function")
        return data[idx : idx + 1]

    @classmethod
    def get_len(cls, df):
        if not WITH_PANDAS:
            raise Exception("DataFrames prototype requires pandas to function")
        return len(df.index)


# When you build own implementation just override it with dataframe_wrapper.set_df_wrapper(new_wrapper_class)
default_wrapper = PandasWrapper

def get_df_wrapper():
    return default_wrapper


def set_df_wrapper(wrapper):
    global default_wrapper
    default_wrapper = wrapper


def create_dataframe(data, columns=None):
    wrapper = get_df_wrapper()
    return wrapper.create_dataframe(data, columns)


def is_dataframe(data):
    wrapper = get_df_wrapper()
    return wrapper.is_dataframe(data)


def is_column(data):
    wrapper = get_df_wrapper()
    return wrapper.is_column(data)


def concat(buffer):
    wrapper = get_df_wrapper()
    return wrapper.concat(buffer)


def iterate(data):
    wrapper = get_df_wrapper()
    return wrapper.iterate(data)


def get_item(data, idx):
    wrapper = get_df_wrapper()
    return wrapper.get_item(data, idx)


def get_len(df):
    wrapper = get_df_wrapper()
    return wrapper.get_len(df)
