package oauth

import (
	"auth/internal/config"
	"context"
	"encoding/json"
	"errors"
	"io"
	"net/http"

	"golang.org/x/oauth2"
	"golang.org/x/oauth2/github"
	"golang.org/x/oauth2/yandex"
)

type OAuthService struct {
	githubConfig *oauth2.Config
	yandexConfig *oauth2.Config
}
type OAuthUser struct {
	ID          string
	Email       string
	DisplayName string
	Provider    string
}

func NewOAuthService(cfg *config.Config) *OAuthService {
	return &OAuthService{
		githubConfig: &oauth2.Config{
			ClientID:     cfg.GitHubClientID,
			ClientSecret: cfg.GitHubClientSecret,
			RedirectURL:  cfg.GitHubRedirectURL,
			Scopes:       []string{"user:email"},
			Endpoint:     github.Endpoint,
		},
		yandexConfig: &oauth2.Config{
			ClientID:     cfg.YandexClientID,
			ClientSecret: cfg.YandexClientSecret,
			RedirectURL:  cfg.YandexRedirectURL,
			Scopes:       []string{"login:email", "login:info"},
			Endpoint:     yandex.Endpoint,
		},
	}
}

// GetAuthURL возвращает URL для аутентификации
func (s *OAuthService) GetAuthURL(provider, state string) (string, error) {
	switch provider {
	case "github":
		return s.githubConfig.AuthCodeURL(state), nil
	case "yandex":
		return s.yandexConfig.AuthCodeURL(state), nil
	default:
		return "", errors.New("unknown provider")
	}
}

// ExchangeCode обменивает код на информацию о пользователе
func (s *OAuthService) ExchangeCode(ctx context.Context, provider string, code string) (*OAuthUser, error) {
	switch provider {
	case "github":
		return s.exchangeGitHubCode(ctx, code)
	case "yandex":
		return s.exchangeYandexCode(ctx, code)
	default:
		return nil, errors.New("unknown provider")
	}
}

func (s *OAuthService) exchangeGitHubCode(ctx context.Context, code string) (*OAuthUser, error) {
	token, err := s.githubConfig.Exchange(ctx, code)
	if err != nil {
		return nil, err
	}

	client := s.githubConfig.Client(ctx, token)

	// Получаем информацию о пользователе
	resp, err := client.Get("https://api.github.com/user")
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var githubUser struct {
		ID    int    `json:"id"`
		Login string `json:"login"`
		Name  string `json:"name"`
	}

	if err := json.Unmarshal(body, &githubUser); err != nil {
		return nil, err
	}

	// Получаем email
	email, err := s.getGitHubEmail(ctx, client)
	if err != nil {
		return nil, err
	}

	return &OAuthUser{
		Email:       email,
		DisplayName: githubUser.Name,
		Provider:    "github",
	}, nil
}

func (s *OAuthService) getGitHubEmail(ctx context.Context, client *http.Client) (string, error) {
	resp, err := client.Get("https://api.github.com/user/emails")
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	var emails []struct {
		Email    string `json:"email"`
		Primary  bool   `json:"primary"`
		Verified bool   `json:"verified"`
	}

	if err := json.Unmarshal(body, &emails); err != nil {
		return "", err
	}

	for _, email := range emails {
		if email.Primary && email.Verified {
			return email.Email, nil
		}
	}

	if len(emails) > 0 {
		return emails[0].Email, nil
	}

	return "", errors.New("github email not found")
}

func (s *OAuthService) exchangeYandexCode(ctx context.Context, code string) (*OAuthUser, error) {
	token, err := s.yandexConfig.Exchange(ctx, code)
	if err != nil {
		return nil, err
	}

	client := s.yandexConfig.Client(ctx, token)

	resp, err := client.Get("https://login.yandex.ru/info?format=json")
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var yandexUser struct {
		ID           string `json:"id"`
		Login        string `json:"login"`
		DefaultEmail string `json:"default_email"`
		DisplayName  string `json:"display_name"`
	}

	if err := json.Unmarshal(body, &yandexUser); err != nil {
		return nil, err
	}

	return &OAuthUser{
		Email:       yandexUser.DefaultEmail,
		DisplayName: yandexUser.DisplayName,
		Provider:    "yandex",
	}, nil
}
