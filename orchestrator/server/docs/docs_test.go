package docs

import "testing"

func TestSwaggerInfoRegistered(t *testing.T) {
	if SwaggerInfo == nil {
		t.Fatal("SwaggerInfo should be initialized")
	}
	if SwaggerInfo.Title != "Wingman Orchestrator API" {
		t.Errorf("unexpected title: %q", SwaggerInfo.Title)
	}
	if SwaggerInfo.BasePath != "/api" {
		t.Errorf("unexpected base path: %q", SwaggerInfo.BasePath)
	}
	if SwaggerInfo.InstanceName() == "" {
		t.Error("instance name should not be empty")
	}
	if docTemplate == "" {
		t.Error("doc template should not be empty")
	}
}
