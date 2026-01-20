package config

import (
 	"log"
 	"os"
 	"strconv"
 	"time"
)

type Config struct {
 	Port       string
 	CookieName string
 	DevMode    bool
 	Mock       bool

 	RedisAddr     string
 	RedisPassword string
 	RedisDB       int
 	SessionTTL    time.Duration

 	AuthURL string
 	MainURL string
}

func MustLoad() *Config {
 	cfg := &Config{
  		Port:       getEnv("PORT", "8080"),
  		CookieName: getEnv("COOKIE_NAME", "session_id"),
  		DevMode:    getEnv("DEV_MODE", "true") == "true",
  		Mock:       getEnv("MOCK", "true") == "true",

  		RedisAddr:     getEnv("REDIS_ADDR", "localhost:6379"),
  		RedisPassword: getEnv("REDIS_PASSWORD", ""),
  		RedisDB:       0,
  		SessionTTL:    24 * time.Hour,

  		AuthURL: getEnv("AUTH_URL", "http://localhost:8081"),
  		MainURL: getEnv("MAIN_URL", "http://localhost:8082"),
 	}

 	if v := os.Getenv("REDIS_DB"); v != "" {
  		if n, err := strconv.Atoi(v); err == nil {
   			cfg.RedisDB = n
  		}
 	}

 	if v := os.Getenv("SESSION_TTL_MIN"); v != "" {
  		if mins, err := strconv.Atoi(v); err == nil && mins > 0 {
   			cfg.SessionTTL = time.Duration(mins) * time.Minute
  		}
 	}

 	log.Printf("Config: PORT=%s MOCK=%v REDIS=%s", cfg.Port, cfg.Mock, cfg.RedisAddr)
 	return cfg
}

func getEnv(key, def string) string {
 	v := os.Getenv(key)
 	if v == "" {
  		return def
 	}
 	return v
}