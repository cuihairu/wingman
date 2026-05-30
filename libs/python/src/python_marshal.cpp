#include "wingman/python/python_marshal.hpp"

namespace wingman {
namespace python {

py::object toPythonObject(const script::ScriptValue& value) {
	switch (value.type) {
	case script::ScriptValue::Null:
		return py::none();
	case script::ScriptValue::Bool:
		return py::bool_(value.boolVal);
	case script::ScriptValue::Int:
		return py::int_(value.intVal);
	case script::ScriptValue::Float:
		return py::float_(value.floatVal);
	case script::ScriptValue::String:
		return py::str(value.strVal);
	case script::ScriptValue::Array: {
		py::list lst(value.arrayVal.size());
		for (size_t i = 0; i < value.arrayVal.size(); ++i) {
		 lst[static_cast<py::ssize_t>(i)] = toPythonObject(value.arrayVal[i]);
		}
		return lst;
	}
	case script::ScriptValue::Object: {
		py::dict dct;
		for (const auto& [k, v] : value.objectVal) {
			dct[py::str(k)] = toPythonObject(v);
		}
		return dct;
	}
	case script::ScriptValue::Callable: {
		// Wrap C++ callable as Python function
		return py::cpp_function([callable = value.callableVal](py::args args) -> py::object {
			std::vector<script::ScriptValue> svArgs;
			svArgs.reserve(args.size());
			for (py::ssize_t i = 0; i < args.size(); ++i) {
				svArgs.push_back(toScriptValue(args[i].cast<py::object>()));
			}
			return toPythonObject(callable(svArgs));
		});
	}
	}
	return py::none();
}

script::ScriptValue toScriptValue(const py::object& obj) {
	if (obj.is_none()) {
		return script::ScriptValue::null();
	}

	try {
		if (py::isinstance<py::bool_>(obj)) {
			return script::ScriptValue::fromBool(obj.cast<bool>());
		}
		if (py::isinstance<py::int_>(obj)) {
			return script::ScriptValue::fromInt(obj.cast<int64_t>());
		}
		if (py::isinstance<py::float_>(obj)) {
			return script::ScriptValue::fromFloat(obj.cast<double>());
		}
		if (py::isinstance<py::str>(obj)) {
			return script::ScriptValue::fromString(obj.cast<std::string>());
		}
		if (py::isinstance<py::list>(obj)) {
			return listToScriptValue(obj.cast<py::list>());
		}
		if (py::isinstance<py::dict>(obj)) {
			return dictToScriptValue(obj.cast<py::dict>());
		}
		if (py::isinstance<py::tuple>(obj)) {
			// tuple → array
			py::tuple tpl = obj.cast<py::tuple>();
			std::vector<script::ScriptValue> arr;
			arr.reserve(tpl.size());
			for (py::ssize_t i = 0; i < tpl.size(); ++i) {
				arr.push_back(toScriptValue(tpl[i].cast<py::object>()));
			}
			return script::ScriptValue::fromArray(std::move(arr));
		}
		// Check if callable (function, lambda, etc.)
		if (py::isinstance<py::function>(obj) || PyObject_HasAttrString(obj.ptr(), "__call__")) {
			// Keep a reference to the Python object
			py::object pyCallable = obj;
			return script::ScriptValue::fromCallable([pyCallable](const std::vector<script::ScriptValue>& args) -> script::ScriptValue {
				py::gil_scoped_acquire gil;
				try {
					// Convert ScriptValue args to Python tuple
					py::tuple pyArgs(args.size());
					for (size_t i = 0; i < args.size(); ++i) {
						pyArgs[i] = toPythonObject(args[i]);
					}
					// Call the Python callable
					py::object result = pyCallable(*pyArgs);
					// Convert result back to ScriptValue
					return toScriptValue(result);
				} catch (const py::error_already_set& e) {
					// Return null on exception
					return script::ScriptValue::null();
				}
			}, true);  // Python callables are thread-safe (due to GIL)
		}
	} catch (const py::error_already_set&) {
		return script::ScriptValue::null();
	}

	return script::ScriptValue::null();
}

script::ScriptValue listToScriptValue(const py::list& lst) {
	std::vector<script::ScriptValue> arr;
	arr.reserve(lst.size());
	for (py::ssize_t i = 0; i < lst.size(); ++i) {
		arr.push_back(toScriptValue(lst[i].cast<py::object>()));
	}
	return script::ScriptValue::fromArray(std::move(arr));
}

script::ScriptValue dictToScriptValue(const py::dict& dct) {
	std::unordered_map<std::string, script::ScriptValue> obj;
	for (auto item : dct) {
		if (py::isinstance<py::str>(item.first)) {
			std::string key = item.first.cast<std::string>();
			obj[key] = toScriptValue(item.second.cast<py::object>());
		}
	}
	return script::ScriptValue::fromObject(std::move(obj));
}

} // namespace python
} // namespace wingman
