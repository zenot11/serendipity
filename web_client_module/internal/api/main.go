package api

import (
 	"bytes"
 	"fmt"
 	"io"
 	"net/http"
 	"time"
)

type MainClient struct {
 	base string
 	mock bool
 	http *http.Client
}

func NewMainClient(base string, mock bool) *MainClient {
 	return &MainClient{
  		base: base,
  		mock: mock,
  		http: &http.Client{Timeout: 8 * time.Second},
 	}
}

func (m *MainClient) Do(method, path, accessToken string, body []byte) (int, string, error) {
 	if m.mock {
  		if path == "/demo/401" {
   			return 401, {"error":"token expired (demo)"}, nil
  		}
  		if path == "/demo/403" {
   			return 403, {"error":"forbidden (demo)"}, nil
		}
  		return 200, fmt.Sprintf(`{"ok":true,"method":"%s","path":"%s"}`, method, path), nil
 	}

 	var reader io.Reader
 	if body != nil && len(body) > 0 {
  		reader = bytes.NewReader(body)
 	}

 	req, err := http.NewRequest(method, m.base+path, reader)
 	if err != nil {
  		return 0, "", err
 	}

 	req.Header.Set("Authorization", "Bearer "+accessToken)
 	req.Header.Set("Content-Type", "application/json")

 	resp, err := m.http.Do(req)
 	if err != nil {
  		return 0, "", err
 	}
 	defer resp.Body.Close()

 	b, _ := io.ReadAll(resp.Body)
 	return resp.StatusCode, string(b), nil
}
