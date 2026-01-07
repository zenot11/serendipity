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

	// Cleanup
	CleanupInterval time.Duration // 5 минут
}

func Load() *Config {
	_ = godotenv.Load()

	return &Config{
		ServerPort: getEnv("SERVER_PORT", "8081"),
		JWTSecret:  getEnv("JWT_SECRET", "super-secret-key-change-in-production"),

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
