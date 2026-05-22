#pragma once

#include <pybind11/pybind11.h>
#include "wingman/script/iscript_engine.hpp"

namespace py = pybind11;

namespace wingman {
namespace python {

// ScriptValue → py::object
py::object toPythonObject(const script::ScriptValue& value);

// py::object → ScriptValue
script::ScriptValue toScriptValue(const py::object& obj);

// py::list/py::dict → ScriptValue
script::ScriptValue listToScriptValue(const py::list& lst);
script::ScriptValue dictToScriptValue(const py::dict& dct);

} // namespace python
} // namespace wingman
