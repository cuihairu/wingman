#include "wingman/event.hpp"
#include "wingman/script/iscript_engine.hpp"

#include <mutex>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>
#include <functional>

namespace wingman {
namespace script {
namespace modules {

namespace {

// FSM state machine implementation
class StateMachine {
public:
	struct StateConfig {
		ScriptValue::CallableFunc onEnter;
		ScriptValue::CallableFunc onExit;

		StateConfig() = default;
		StateConfig(ScriptValue::CallableFunc enter, ScriptValue::CallableFunc exit)
			: onEnter(std::move(enter)), onExit(std::move(exit)) {}
	};

	struct TransitionConfig {
		std::string event;
		ScriptValue::CallableFunc guard;
		ScriptValue::CallableFunc action;

		TransitionConfig() = default;
		TransitionConfig(std::string evt, ScriptValue::CallableFunc g, ScriptValue::CallableFunc a)
			: event(std::move(evt)), guard(std::move(g)), action(std::move(a)) {}
	};

	struct Transition {
		std::string from;
		std::string to;
		TransitionConfig config;
		uint64_t id;

		Transition() = default;
		Transition(std::string f, std::string t, TransitionConfig cfg, uint64_t i)
			: from(std::move(f)), to(std::move(t)), config(std::move(cfg)), id(i) {}
	};

	StateMachine(std::string name, std::string initial)
		: name_(std::move(name)), initialState_(initial), currentState_(std::move(initial)) {
		// Initialize the initial state if not yet defined
		if (states_.find(currentState_) == states_.end()) {
			states_[currentState_] = StateConfig();
		}
	}

	void state(const std::string& name, ScriptValue::CallableFunc onEnter, ScriptValue::CallableFunc onExit) {
		states_[name] = StateConfig(std::move(onEnter), std::move(onExit));
	}

	void transition(const std::string& from, const std::string& to,
	              const std::string& on, ScriptValue::CallableFunc guard, ScriptValue::CallableFunc action) {
		uint64_t id = nextTransitionId_++;
		TransitionConfig config(on, std::move(guard), std::move(action));
		transitions_.push_back(Transition(from, to, std::move(config), id));
		// Index by event for faster lookup
		eventIndex_[on].push_back(static_cast<int>(transitions_.size()) - 1);
	}

	bool dispatch(const std::string& event, const nlohmann::json& payload) {
		auto it = eventIndex_.find(event);
		if (it == eventIndex_.end()) {
			return false; // No transitions for this event
		}

		nlohmann::json context = nlohmann::json::object();
		context["from"] = currentState_;
		context["event"] = event;
		context["payload"] = payload;

		for (int idx : it->second) {
			const auto& trans = transitions_[idx];
			if (trans.from != currentState_) continue;

			// Check guard if present
			if (trans.config.guard) {
				ScriptValue result = trans.config.guard({
					ScriptValue::fromObject({
						{"from", ScriptValue::fromString(trans.from)},
						{"to", ScriptValue::fromString(trans.to)},
						{"event", ScriptValue::fromString(event)},
						{"payload", fromJson(payload)}
					})
				});
				if (!result.asBool(false)) {
					continue; // Guard rejected
				}
			}

			// Execute on_exit of current state
			auto stateIt = states_.find(currentState_);
			if (stateIt != states_.end() && stateIt->second.onExit) {
				stateIt->second.onExit({
					ScriptValue::fromObject({
						{"state", ScriptValue::fromString(currentState_)},
						{"event", ScriptValue::fromString(event)},
						{"payload", fromJson(payload)}
					})
				});
			}

			// Execute action if present
			if (trans.config.action) {
				trans.config.action({
					ScriptValue::fromObject({
						{"from", ScriptValue::fromString(trans.from)},
						{"to", ScriptValue::fromString(trans.to)},
						{"event", ScriptValue::fromString(event)},
						{"payload", fromJson(payload)}
					})
				});
			}

			// Change state
			std::string oldState = currentState_;
			currentState_ = trans.to;

			// Execute on_enter of new state
			auto newStateIt = states_.find(currentState_);
			if (newStateIt != states_.end() && newStateIt->second.onEnter) {
				newStateIt->second.onEnter({
					ScriptValue::fromObject({
						{"state", ScriptValue::fromString(currentState_)},
						{"from", ScriptValue::fromString(oldState)},
						{"event", ScriptValue::fromString(event)},
						{"payload", fromJson(payload)}
					})
				});
			}

			// Emit fsm.changed event
			EventHub::instance().emit("fsm.changed", {
				{"machine", name_},
				{"from", oldState},
				{"to", currentState_},
				{"event", event}
			}, "fsm");

			return true;
		}

		return false; // No matching transition
	}

	std::string current() const {
		return currentState_;
	}

	void reset() {
		currentState_ = initialState_;
	}

	const std::string& name() const { return name_; }

private:
	static ScriptValue fromJson(const nlohmann::json& j) {
		if (j.is_null()) return ScriptValue::null();
		if (j.is_boolean()) return ScriptValue::fromBool(j.get<bool>());
		if (j.is_number_integer()) return ScriptValue::fromInt(j.get<int64_t>());
		if (j.is_number_float()) return ScriptValue::fromFloat(j.get<double>());
		if (j.is_string()) return ScriptValue::fromString(j.get<std::string>());
		if (j.is_array()) {
			std::vector<ScriptValue> arr;
			arr.reserve(j.size());
			for (const auto& item : j) {
				arr.push_back(fromJson(item));
			}
			return ScriptValue::fromArray(std::move(arr));
		}
		if (j.is_object()) {
			std::unordered_map<std::string, ScriptValue> obj;
			for (auto it = j.begin(); it != j.end(); ++it) {
				obj[it.key()] = fromJson(it.value());
			}
			return ScriptValue::fromObject(std::move(obj));
		}
		return ScriptValue::null();
	}

	std::string name_;
	std::string initialState_;
	std::string currentState_;
	std::unordered_map<std::string, StateConfig> states_;
	std::vector<Transition> transitions_;
	std::unordered_map<std::string, std::vector<int>> eventIndex_;
	uint64_t nextTransitionId_ = 1;
};

// Global state machine registry (using shared_ptr for safe lock-free access)
std::unordered_map<std::string, std::shared_ptr<StateMachine>> g_stateMachines;
uint64_t g_nextMachineId = 1;
std::mutex g_stateMachinesMutex;  // Protects g_stateMachines and g_nextMachineId

// Helper to convert ScriptValue to JSON
nlohmann::json toJson(const ScriptValue& value) {
	switch (value.type) {
	case ScriptValue::Null:
		return nullptr;
	case ScriptValue::Bool:
		return value.boolVal;
	case ScriptValue::Int:
		return value.intVal;
	case ScriptValue::Float:
		return value.floatVal;
	case ScriptValue::String:
		return value.strVal;
	case ScriptValue::Array: {
		nlohmann::json arr = nlohmann::json::array();
		for (const auto& item : value.arrayVal) {
			arr.push_back(toJson(item));
		}
		return arr;
	}
	case ScriptValue::Object: {
		nlohmann::json obj = nlohmann::json::object();
		for (const auto& [key, item] : value.objectVal) {
			obj[key] = toJson(item);
		}
		return obj;
	}
	default:
		return nullptr;
	}
}

// Cleanup function for global state (implementation)
void cleanupFsmModuleInternal() {
	std::lock_guard<std::mutex> lock(g_stateMachinesMutex);
	g_stateMachines.clear();
	g_nextMachineId = 1;
}

} // namespace

// Cleanup function for global state (called during engine shutdown)
void cleanupFsmModule() {
	cleanupFsmModuleInternal();
}

ModuleDescriptor createFsmModule() {
	ModuleDescriptor mod;
	mod.name = "fsm";

	// create(name, initial) -> machineId
	mod.functions.push_back({"create", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);

		std::string name = args[0].asString();
		std::string initial = args[1].asString();

		std::string machineId;
		{
			std::lock_guard<std::mutex> lock(g_stateMachinesMutex);
			machineId = "fsm-" + name + "-" + std::to_string(g_nextMachineId++);
			g_stateMachines[machineId] = std::make_shared<StateMachine>(name, initial);
		}

		return ScriptValue::fromString(machineId);
	}, "name:string, initial:string -> machineId:string"});

	// state(machineId, stateName, onEnter?, onExit?)
	mod.functions.push_back({"state", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);

		std::string machineId = args[0].asString();
		std::string stateName = args[1].asString();

		ScriptValue::CallableFunc onEnter = args.size() > 2 && args[2].isCallable() ? args[2].callableVal : nullptr;
		ScriptValue::CallableFunc onExit = args.size() > 3 && args[3].isCallable() ? args[3].callableVal : nullptr;

		{
			std::lock_guard<std::mutex> lock(g_stateMachinesMutex);
			auto it = g_stateMachines.find(machineId);
			if (it == g_stateMachines.end()) return ScriptValue::fromBool(false);
			it->second->state(stateName, std::move(onEnter), std::move(onExit));
		}
		return ScriptValue::fromBool(true);
	}, "machineId:string, stateName:string, onEnter?:function, onExit?:function -> bool"});

	// transition(machineId, from, to, on?, guard?, action?)
	mod.functions.push_back({"transition", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 3) return ScriptValue::fromBool(false);

		std::string machineId = args[0].asString();
		std::string from = args[1].asString();
		std::string to = args[2].asString();
		std::string on = args.size() > 3 && args[3].isString() ? args[3].asString() : "";

		ScriptValue::CallableFunc guard = args.size() > 4 && args[4].isCallable() ? args[4].callableVal : nullptr;
		ScriptValue::CallableFunc action = args.size() > 5 && args[5].isCallable() ? args[5].callableVal : nullptr;

		{
			std::lock_guard<std::mutex> lock(g_stateMachinesMutex);
			auto it = g_stateMachines.find(machineId);
			if (it == g_stateMachines.end()) return ScriptValue::fromBool(false);
			it->second->transition(from, to, on, std::move(guard), std::move(action));
		}
		return ScriptValue::fromBool(true);
	}, "machineId:string, from:string, to:string, on?:string, guard?:function, action?:function -> bool"});

	// dispatch(machineId, event, payload?)
	mod.functions.push_back({"dispatch", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);

		std::string machineId = args[0].asString();
		std::string event = args[1].asString();
		nlohmann::json payload = args.size() > 2 ? toJson(args[2]) : nlohmann::json::object();

		// Find state machine and copy shared_ptr under lock
		std::shared_ptr<StateMachine> machine;
		{
			std::lock_guard<std::mutex> lock(g_stateMachinesMutex);
			auto it = g_stateMachines.find(machineId);
			if (it == g_stateMachines.end()) return ScriptValue::fromBool(false);
			machine = it->second;
		}
		// Call dispatch() after releasing lock to avoid deadlock on callbacks
		bool result = machine->dispatch(event, payload);
		return ScriptValue::fromBool(result);
	}, "machineId:string, event:string, payload?:object -> bool"});

	// current(machineId) -> stateName
	mod.functions.push_back({"current", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::null();

		std::string machineId = args[0].asString();

		std::string result;
		{
			std::lock_guard<std::mutex> lock(g_stateMachinesMutex);
			auto it = g_stateMachines.find(machineId);
			if (it == g_stateMachines.end()) return ScriptValue::null();
			result = it->second->current();
		}
		return ScriptValue::fromString(result);
	}, "machineId:string -> stateName:string"});

	// reset(machineId)
	mod.functions.push_back({"reset", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);

		std::string machineId = args[0].asString();

		{
			std::lock_guard<std::mutex> lock(g_stateMachinesMutex);
			auto it = g_stateMachines.find(machineId);
			if (it == g_stateMachines.end()) return ScriptValue::fromBool(false);
			it->second->reset();
		}
		return ScriptValue::fromBool(true);
	}, "machineId:string -> bool"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
