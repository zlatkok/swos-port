class StringTable:
    def __init__(self, variable, native, declareIndexVariable=True, values=(), initialValue=0, nativeFlags=()):
        assert isinstance(variable, str)
        assert isinstance(values, (list, tuple))
        assert isinstance(initialValue, int)
        assert isinstance(nativeFlags, (list, tuple))

        self.variable = variable
        self.native = native
        self.declareIndexVariable = declareIndexVariable
        self.values = values
        self.initialValue = initialValue
        self.nativeFlags = nativeFlags      # first flag is for index variable, rest are for values
