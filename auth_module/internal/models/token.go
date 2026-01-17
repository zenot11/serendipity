package models

// AccessTokenClaims Access Token содержит только permissions
type AccessTokenClaims struct {
	Permissions []string `json:"permissions"`
	ExpiresAt   int64    `json:"exp"`
}

// RefreshTokenClaims Refresh Token содержит только email
type RefreshTokenClaims struct {
	Email     string `json:"email"`
	ExpiresAt int64  `json:"exp"`
}

type TokenPair struct {
	AccessToken  string `json:"access_token"`
	RefreshToken string `json:"refresh_token"`
	TokenType    string `json:"token_type"`
	ExpiresIn    int    `json:"expires_in"`
}

// LoginRequest login_token
type LoginRequest struct {
	Type       string `json:"type" form:"type" binding:"required,oneof=github yandex code"`
	LoginToken string `json:"login_token" form:"login_token" binding:"required"`
}

// CodeAuthRequest для кодовой аутентификации
type CodeAuthRequest struct {
	Code         string `json:"code" form:"code" binding:"required"`
	RefreshToken string `json:"refresh_token" form:"refresh_token" binding:"required"`
}

type RefreshRequest struct {
	RefreshToken string `json:"refresh_token" form:"refresh_token" binding:"required"`
}

type LogoutRequest struct {
	RefreshToken string `json:"refresh_token" form:"refresh_token" binding:"required"`
	All          bool   `json:"all" form:"all"`
}

type CheckLoginRequest struct {
	LoginToken string `json:"login_token" form:"login_token" binding:"required"`
}

type LoginStatusResponse struct {
	Status       string `json:"status"` // "pending", "granted", "denied"
	AccessToken  string `json:"access_token,omitempty"`
	RefreshToken string `json:"refresh_token,omitempty"`
}

type SuccessResponse struct {
	Message string `json:"message"`
}

type ErrorResponse struct {
	Error string `json:"error"`
}
