package workflow

import (
	"encoding/json"
	"fmt"
	"reflect"
	"strings"
)

func evaluateCondition(params map[string]any) (bool, string, error) {
	if v, ok := params["expression"]; ok {
		result, ok := toBool(v)
		if !ok {
			return false, "", fmt.Errorf("condition expression must be boolean-compatible")
		}
		if result {
			return true, "condition matched", nil
		}
		return false, "condition expression is false", nil
	}

	actual, ok := params["value"]
	if !ok {
		actual = params["actual"]
	}
	operator := "truthy"
	if op, ok := params["operator"].(string); ok && strings.TrimSpace(op) != "" {
		operator = op
	} else if op, ok := params["op"].(string); ok && strings.TrimSpace(op) != "" {
		operator = op
	}
	operator = normalizeConditionOperator(operator)
	expected, hasExpected := params["expected"]

	var matched bool
	var err error
	switch operator {
	case "truthy":
		matched, ok = toBool(actual)
		if !ok {
			return false, "", fmt.Errorf("condition value is not boolean-compatible")
		}
	case "falsy":
		matched, ok = toBool(actual)
		if !ok {
			return false, "", fmt.Errorf("condition value is not boolean-compatible")
		}
		matched = !matched
	case "eq":
		if !hasExpected {
			return false, "", fmt.Errorf("condition operator eq requires expected")
		}
		matched = valuesEqual(actual, expected)
	case "ne":
		if !hasExpected {
			return false, "", fmt.Errorf("condition operator ne requires expected")
		}
		matched = !valuesEqual(actual, expected)
	case "gt", "gte", "lt", "lte":
		if !hasExpected {
			return false, "", fmt.Errorf("condition operator %s requires expected", operator)
		}
		matched, err = compareNumbers(actual, expected, operator)
		if err != nil {
			return false, "", err
		}
	case "contains", "not_contains":
		if !hasExpected {
			return false, "", fmt.Errorf("condition operator %s requires expected", operator)
		}
		matched = containsValue(actual, expected)
		if operator == "not_contains" {
			matched = !matched
		}
	default:
		return false, "", fmt.Errorf("unknown condition operator %q", operator)
	}

	if matched {
		return true, fmt.Sprintf("condition matched: %s", operator), nil
	}
	return false, fmt.Sprintf("condition failed: %s", operator), nil
}

func normalizeConditionOperator(op string) string {
	switch strings.ToLower(strings.TrimSpace(op)) {
	case "", "true", "truthy", "is_true":
		return "truthy"
	case "false", "falsy", "is_false":
		return "falsy"
	case "=", "==", "eq", "equals":
		return "eq"
	case "!=", "<>", "ne", "not_equals":
		return "ne"
	case ">", "gt":
		return "gt"
	case ">=", "gte":
		return "gte"
	case "<", "lt":
		return "lt"
	case "<=", "lte":
		return "lte"
	case "contains", "in":
		return "contains"
	case "not_contains", "not-contains", "not in", "not_in":
		return "not_contains"
	default:
		return strings.ToLower(strings.TrimSpace(op))
	}
}

func valuesEqual(a, b any) bool {
	if af, ok := toFloat(a); ok {
		if bf, ok := toFloat(b); ok {
			return af == bf
		}
	}
	if ab, ok := toBool(a); ok {
		if bb, ok := toBool(b); ok {
			return ab == bb
		}
	}
	return fmt.Sprint(a) == fmt.Sprint(b)
}

func compareNumbers(actual, expected any, operator string) (bool, error) {
	left, ok := toFloat(actual)
	if !ok {
		return false, fmt.Errorf("condition actual value %q is not numeric", fmt.Sprint(actual))
	}
	right, ok := toFloat(expected)
	if !ok {
		return false, fmt.Errorf("condition expected value %q is not numeric", fmt.Sprint(expected))
	}
	switch operator {
	case "gt":
		return left > right, nil
	case "gte":
		return left >= right, nil
	case "lt":
		return left < right, nil
	case "lte":
		return left <= right, nil
	default:
		return false, fmt.Errorf("unknown numeric operator %q", operator)
	}
}

func containsValue(actual, expected any) bool {
	if actual == nil {
		return false
	}
	if s, ok := actual.(string); ok {
		return strings.Contains(s, fmt.Sprint(expected))
	}
	value := reflect.ValueOf(actual)
	if value.Kind() == reflect.Slice || value.Kind() == reflect.Array {
		for i := 0; i < value.Len(); i++ {
			if valuesEqual(value.Index(i).Interface(), expected) {
				return true
			}
		}
		return false
	}
	return strings.Contains(fmt.Sprint(actual), fmt.Sprint(expected))
}

func toBool(v any) (bool, bool) {
	switch b := v.(type) {
	case bool:
		return b, true
	case string:
		switch strings.ToLower(strings.TrimSpace(b)) {
		case "true", "1", "yes", "y", "on":
			return true, true
		case "false", "0", "no", "n", "off", "":
			return false, true
		}
	case float64:
		return b != 0, true
	case float32:
		return b != 0, true
	case int:
		return b != 0, true
	case int64:
		return b != 0, true
	case json.Number:
		f, err := b.Float64()
		return f != 0, err == nil
	}
	return false, false
}
