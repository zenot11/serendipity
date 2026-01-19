package api

import (
 	"bytes"
 	"encoding/json"
 	"fmt"
 	"net/http"
 	"time"
)

type AuthClient struct {
 	base string
 	mock bool
 	http *http.Client
}

func NewAuthClient(base string, mock bool) *AuthClient {
 	return &AuthClient{
  		base: base,
  		mock: mock,
  		http: &http.Client{Timeout: 5 * time.Second},
 	}
}-

type StartLoginResponse struct {
 	Type string json:"type" 
	 URL  string json:"url"  
 	Code string json:"code" 
}

type CheckLoginResponse struct {
 	Status string json:"status" 
 	AccessToken string json:"access_token,omitempty"
 	RefreshToken string json:"refresh_token,omitempty"
}

type RefreshRequest struct {
 	RefreshToken string json:"refresh_token"
}
type RefreshResponse struct {
 	AccessToken string json:"access_token"
 	RefreshToken string json:"refresh_token"
}

type LogoutRequest struct {
 	RefreshToken string json:"refresh_token"
}

func (a *AuthClient) StartLogin(loginType, loginToken string) (*StartLoginResponse, error) {
 	if a.mock {
  		if loginType == "code" {
   			return &StartLoginResponse{Type: "code", Code: "123456"}, nil
  		}
  		return &StartLoginResponse{Type: loginType, URL: "/"}, nil
 	}

	req, err := http.NewRequest("GET", fmt.Sprintf("%s/login/start?type=%s&state=%s", a.base, loginType, loginToken), nil, )
	if err != nil {
 	 	return nil, err
 	}

	resp, err := a.http.Do(req)
 	if err != nil {
  		return nil, err
 	}
 	defer resp.Body.Close()

 	if resp.StatusCode >= 300 {
  		return nil, fmt.Errorf("auth StartLogin bad status: %s", resp.Status)
 	}

 	var out StartLoginResponse
 	if err := json.NewDecoder(resp.Body).Decode(&out); err != nil {
  		return nil, err
 	}
 	return &out, nil
}

func (a *AuthClient) CheckLogin(loginToken string) (*CheckLoginResponse, error) {
 	if a.mock {
  		return &CheckLoginResponse{
   			Status: "success",
   			AccessToken: "mock_access_token",
   			RefreshToken: "mock_refresh_token",
  		}, nil
 	}

 	req, err := http.NewRequest("GET",
  	fmt.Sprintf("%s/login/check?state=%s", a.base, loginToken),
  	nil,
 	)
 	if err != nil {
  		return nil, err
 	}

 	resp, err := a.http.Do(req)
 	if err != nil {
  		return nil, err
 	}
 	defer resp.Body.Close()

 	if resp.StatusCode >= 300 {
  		return nil, fmt.Errorf("auth CheckLogin bad status: %s", resp.Status)
 	}

 	var out CheckLoginResponse
 	if err := json.NewDecoder(resp.Body).Decode(&out); err != nil {
  		return nil, err
 	}
 	return &out, nil
}

func (a *AuthClient) Refresh(refreshToken string) (*RefreshResponse, error) {
 	if a.mock {
  		return &RefreshResponse{
   			AccessToken: "mock_access_token_new",
   			RefreshToken: "mock_refresh_token_new",
  		}, nil
 	}

 	body, _ := json.Marshal(RefreshRequest{RefreshToken: refreshToken})
 	resp, err := a.http.Post(a.base+"/refresh", "application/json", bytes.NewReader(body))
 	if err != nil {
  		return nil, err
 	}
 	defer resp.Body.Close()

 	if resp.StatusCode == 401 {
  		return nil, fmt.Errorf("refresh unauthorized")
 	}
 	if resp.StatusCode >= 300 {
  		return nil, fmt.Errorf("refresh bad status: %s", resp.Status)
 	}

 	var out RefreshResponse
 	if err := json.NewDecoder(resp.Body).Decode(&out); err != nil {
  		return nil, err
 	}
 	return &out, nil
}

func (a *AuthClient) LogoutAll(refreshToken string) error {
 	if a.mock {
  		return nil
 	}

 	body, _ := json.Marshal(LogoutRequest{RefreshToken: refreshToken})
 	resp, err := a.http.Post(a.base+"/logout", "application/json", bytes.NewReader(body))
 	if err != nil {
  		return err
 	}
 	defer resp.Body.Close()

 	if resp.StatusCode >= 300 {
  		return fmt.Errorf("logout bad status: %s", resp.Status)
 	}
 	return nil
}