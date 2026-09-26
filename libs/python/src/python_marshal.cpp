#include "wingman/python/python_marshal.hpp"

#include <pybind11/gil_simple.h>
#include <memory>

namespace wingman {
namespace python {

py::object toPythonObject(const script::ScriptValue& value) {
	switch (value.type) {
	case script::ScriptValue::Type::Null:
		return py::none();
	case script::ScriptValue::Type::Bool:
		return py::bool_(value.boolVal);
	case script::ScriptValue::Type::Int:
		return py::int_(value.intVal);
	case script::ScriptValue::Type::Float:
		return py::float_(value.floatVal);
	case script::ScriptValue::Type::String:
		return py::str(value.strVal);
	case script::ScriptValue::Type::Array: {
		py::list lst(value.arrayVal.size());
		for (size_t i = 0; i < value.arrayVal.size(); ++i) {
		 lst[static_cast<py::ssize_t>(i)] = toPythonObject(value.arrayVal[i]);
		}
		return lst;
	}
	case script::ScriptValue::Type::Object: {
		py::dict dct;
		for (const auto& [k, v] : value.objectVal) {
			dct[py::str(k)] = toPythonObject(v);
		}
		return dct;
	}
	case script::ScriptValue::Type::Callable: {
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
			// ScriptValue 的销毁发生在调用方任意时刻、任意线程（不保证持有
			// GIL），而 py::object 析构的 dec_ref 需要 GIL——引用放进带 GIL
			// 守卫 deleter 的 shared_ptr，拷贝共享同一引用，最后一个释放者
			// 在 deleter 里补取 GIL
			auto pyCallable = std::shared_ptr<py::object>(
				new py::object(obj),
				[](py::object* held) {
					PyGILState_STATE gstate = PyGILState_Ensure();
					delete held;
					PyGILState_Release(gstate);
				});
			return script::ScriptValue::fromCallable([pyCallable](const std::vector<script::ScriptValue>& args) -> script::ScriptValue {
				py::gil_scoped_acquire_simple gil;
				try {
					// Convert ScriptValue args to Python tuple
					py::tuple pyArgs(args.size());
					for (size_t i = 0; i < args.size(); ++i) {
						pyArgs[i] = toPythonObject(args[i]);
					}
					// Call the Python callable
					py::object result = (*pyCallable)(*pyArgs);
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
