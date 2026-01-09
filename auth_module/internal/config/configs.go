package config

import (
	"os"
	"time"

	"github.com/joho/godotenv"
)

type Config struct {
	ServerPort string
	JWTSecret  string

	// Время жизни
	LoginTokenTTL   time.Duration // 5 минут
	AccessTokenTTL  time.Duration // 1 минута
	RefreshTokenTTL time.Duration // 7 дней
	AuthCodeTTL     time.Duration // 1 минута

	// OAuth GitHub
	GitHubClientID     string
	GitHubClientSecret string
	GitHubRedirectURL  string

	// OAuth Yandex
	YandexClientID     string
	YandexClientSecret string
	YandexRedirectURL  string

	// Cleanup
	CleanupInterval time.Duration // 5 минут
}

func Load() *Config {
	_ = godotenv.Load()

	return &Config{
		ServerPort: getEnv("SERVER_PORT", "8081"),
		JWTSecret:  getEnv("JWT_SECRET", "super-secret-key-change-in-production"),

		GitHubClientID:     getEnv("GITHUB_CLIENT_ID", ""),
		GitHubClientSecret: getEnv("GITHUB_CLIENT_SECRET", ""),
		GitHubRedirectURL:  getEnv("GITHUB_REDIRECT_URL", "http://localhost:8081/auth/github/callback"),

		YandexClientID:     getEnv("YANDEX_CLIENT_ID", ""),
		YandexClientSecret: getEnv("YANDEX_CLIENT_SECRET", ""),
		YandexRedirectURL:  getEnv("YANDEX_REDIRECT_URL", "http://localhost:8081/auth/yandex/callback"),

		LoginTokenTTL:   5 * time.Minute,    // 5 минут
		AccessTokenTTL:  1 * time.Minute,    // 1 минута
		RefreshTokenTTL: 7 * 24 * time.Hour, // 7 дней
		AuthCodeTTL:     1 * time.Minute,    // 1 минута
		CleanupInterval: 5 * time.Minute,
	}
}

func getEnv(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}
