Notes about refactoring

Check if createDSP() cannot be factored in Target

When reviving older targets:

global replace  getDSPWidths -> getMaxDSPWidths
