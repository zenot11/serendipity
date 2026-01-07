package jwt

import (
	"auth/internal/config"
	"auth/internal/models"
	"time"

	"github.com/golang-jwt/jwt/v5"
)

type JWTService struct {
	secret          []byte
	accessTokenTTL  time.Duration
	refreshTokenTTL time.Duration
}

func NewJWTService(cfg *config.Config) *JWTService {
	return &JWTService{
		secret:          []byte(cfg.JWTSecret),
		accessTokenTTL:  cfg.AccessTokenTTL,
		refreshTokenTTL: cfg.RefreshTokenTTL,
	}
}

// generateAccessToken создает access token
func (s *JWTService) generateAccessToken(permissions []string) (string, error) {
	claims := models.AccessTokenClaims{
		Permissions: permissions,
		ExpiresAt:   time.Now().Add(s.accessTokenTTL).Unix(),
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"permissions": claims.Permissions,
		"exp":         claims.ExpiresAt,
	})

	return token.SignedString(s.secret)
}

// ValidateToken проверяет токен
func (s *JWTService) ValidateToken(tokenString string) (jwt.MapClaims, error) {
	token, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
		return s.secret, nil
	})

	if err != nil {
		return nil, err
	}

	if claims, ok := token.Claims.(jwt.MapClaims); ok && token.Valid {
		return claims, nil
	}

	return nil, jwt.ErrSignatureInvalid
}
