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

// ValidateAccessToken проверяет access token
func (s *JWTService) ValidateAccessToken(tokenString string) (*models.AccessTokenClaims, error) {
	token, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, jwt.ErrSignatureInvalid
		}
		return s.secret, nil
	})
	if err != nil {
		return nil, err
	}

	if claims, ok := token.Claims.(jwt.MapClaims); ok && token.Valid {
		// Извлекаем permissions
		var permissions []string
		if permsInterface, ok := claims["permissions"].([]interface{}); ok {
			for _, p := range permsInterface {
				if perm, ok := p.(string); ok {
					permissions = append(permissions, perm)
				}
			}
		}
		exp, _ := claims["exp"].(float64)

		return &models.AccessTokenClaims{
			Permissions: permissions,
			ExpiresAt:   int64(exp),
		}, nil
	}
	return nil, jwt.ErrSignatureInvalid
}

// generateRefreshToken создает refresh token: только email
func (s *JWTService) generateRefreshToken(email string) (string, error) {
	claims := models.RefreshTokenClaims{
		Email:     email,
		ExpiresAt: time.Now().Add(s.refreshTokenTTL).Unix(),
	}
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"email": claims.Email,
		"exp":   claims.ExpiresAt,
	})
	return token.SignedString(s.secret)
}

// ValidateRefreshToken проверяет refresh token и возвращает email
func (s *JWTService) ValidateRefreshToken(tokenString string) (string, error) {
	token, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, jwt.ErrSignatureInvalid
		}
		return s.secret, nil
	})
	if err != nil {
		return "", err
	}

	claims, ok := token.Claims.(jwt.MapClaims)
	if !ok || !token.Valid {
		return "", jwt.ErrSignatureInvalid
	}

	expFloat, ok := claims["exp"].(float64)
	if !ok {
		return "", jwt.ErrTokenExpired
	}

	expTime := time.Unix(int64(expFloat), 0)
	if time.Now().After(expTime) {
		return "", jwt.ErrTokenExpired
	}

	email, ok := claims["email"].(string)
	if !ok || email == "" {
		return "", jwt.ErrSignatureInvalid
	}

	return email, nil
}

// GenerateTokenPair создает пару токенов
func (s *JWTService) GenerateTokenPair(user *models.User, permissions []string) (*models.TokenPair, error) {
	// Access Token - только permissions
	accessToken, err := s.generateAccessToken(permissions)
	if err != nil {
		return nil, err
	}

	// Refresh Token - только email
	refreshToken, err := s.generateRefreshToken(user.Email)
	if err != nil {
		return nil, err
	}

	return &models.TokenPair{
		AccessToken:  accessToken,
		RefreshToken: refreshToken,
		TokenType:    "Bearer",
		ExpiresIn:    int(s.accessTokenTTL.Seconds()),
	}, nil
}
