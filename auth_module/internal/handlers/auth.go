package handlers

import (
	"auth/internal/database/repository"
	"auth/internal/database/storage"
	"auth/internal/models"
	"auth/internal/pkg/jwt"
	"auth/internal/pkg/oauth"
	"auth/internal/services"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"go.mongodb.org/mongo-driver/bson"
)

type AuthHandler struct {
	jwtService     *jwt.JWTService
	oauthService   *oauth.OAuthService
	codeService    *services.CodeAuthService
	mongoRepo      *storage.MongoRepository
	loginStateMgr  *repository.LoginStateManager
	permissionsSvc *services.PermissionsService
}

func NewAuthHandler(
	jwtService *jwt.JWTService,
	oauthService *oauth.OAuthService,
	codeService *services.CodeAuthService,
	mongoRepo *storage.MongoRepository,
	loginStateMgr *repository.LoginStateManager,
	permissionsSvc *services.PermissionsService,
) *AuthHandler {
	return &AuthHandler{
		jwtService:     jwtService,
		oauthService:   oauthService,
		codeService:    codeService,
		mongoRepo:      mongoRepo,
		loginStateMgr:  loginStateMgr,
		permissionsSvc: permissionsSvc,
	}
}

// InitiateLoginHandler - POST /auth/login
func (h *AuthHandler) InitiateLoginHandler(c *gin.Context) {
	var req models.LoginRequest
	if err := c.ShouldBind(&req); err != nil {
		c.JSON(http.StatusBadRequest, models.ErrorResponse{Error: "Invalid request format"})
		return
	}
	// Создаем состояние входа
	h.loginStateMgr.CreateLoginState(req.LoginToken)

	switch req.Type {
	case "github", "yandex":
		url, err := h.oauthService.GetAuthURL(req.Type, req.LoginToken)
		if err != nil {
			c.JSON(http.StatusBadRequest, models.ErrorResponse{Error: "Invalid auth type"})
			return
		}
		c.JSON(http.StatusOK, gin.H{"auth_url": url})

	case "code":
		code := h.codeService.GenerateCode(req.LoginToken)
		c.JSON(http.StatusOK, gin.H{"code": code})

	default:
		c.JSON(http.StatusBadRequest, models.ErrorResponse{Error: "Unsupported auth type"})
	}
}

// CheckLoginStatusHandler - GET /auth/login/status
func (h *AuthHandler) CheckLoginStatusHandler(c *gin.Context) {
	var req models.CheckLoginRequest
	if err := c.ShouldBind(&req); err != nil {
		c.JSON(http.StatusBadRequest, models.ErrorResponse{Error: "Login token is required"})
		return
	}

	status, exists := h.loginStateMgr.GetFullLoginState(req.LoginToken)
	if !exists {
		c.JSON(http.StatusNotFound, models.ErrorResponse{Error: "Login token not found or expired"})
		return
	}

	response := models.LoginStatusResponse{Status: status.Status}

	// Если статус "granted", возвращаем токены
	if status.Status == "granted" {
		response.AccessToken = status.AccessToken
		response.RefreshToken = status.RefreshToken
	}

	c.JSON(http.StatusOK, response)
}

// RefreshTokenHandler - POST /auth/refresh
func (h *AuthHandler) RefreshTokenHandler(c *gin.Context) {
	var req models.RefreshRequest
	if err := c.ShouldBind(&req); err != nil {
		c.JSON(http.StatusBadRequest, models.ErrorResponse{Error: "Refresh token is required"})
		return
	}

	// Валидируем refresh token
	email, err := h.jwtService.ValidateRefreshToken(req.RefreshToken)
	if err != nil {
		c.JSON(http.StatusUnauthorized, models.ErrorResponse{Error: "Invalid refresh token"})
		return
	}

	// Проверяем наличие refresh token в БД
	ctx := c.Request.Context()
	hasToken, err := h.mongoRepo.HasRefreshToken(ctx, req.RefreshToken)
	if err != nil || !hasToken {
		c.JSON(http.StatusUnauthorized, models.ErrorResponse{Error: "Refresh token not found"})
		return
	}

	// Получаем пользователя
	user, err := h.mongoRepo.GetUserByEmail(ctx, email)
	if err != nil || user == nil {
		c.JSON(http.StatusUnauthorized, models.ErrorResponse{Error: "User not found"})
		return
	}

	// Проверяем блокировку
	if user.IsBlocked {
		c.JSON(http.StatusForbidden, models.ErrorResponse{Error: "User is blocked"})
		return
	}

	// Получаем разрешения
	permissions := h.permissionsSvc.GetUserPermissions(user)

	// Генерируем новую пару токенов
	tokenPair, err := h.jwtService.GenerateTokenPair(user, permissions)
	if err != nil {
		c.JSON(http.StatusInternalServerError, models.ErrorResponse{Error: "Failed to generate tokens"})
		return
	}

	// Удаляем старый refresh token и добавляем новый
	_ = h.mongoRepo.RemoveRefreshToken(ctx, req.RefreshToken)

	refreshExpiry := time.Now().Add(7 * 24 * time.Hour)
	err = h.mongoRepo.AddRefreshToken(ctx, user.ID.Hex(), tokenPair.RefreshToken, refreshExpiry)
	if err != nil {
		c.JSON(http.StatusInternalServerError, models.ErrorResponse{Error: "Failed to save refresh token"})
		return
	}

	c.JSON(http.StatusOK, tokenPair)
}

// LogoutHandler - POST /auth/logout
func (h *AuthHandler) LogoutHandler(c *gin.Context) {
	var req models.LogoutRequest
	if err := c.ShouldBind(&req); err != nil {
		c.JSON(http.StatusBadRequest, models.ErrorResponse{Error: "Refresh token is required"})
		return
	}
	ctx := c.Request.Context()
	email, err := h.jwtService.ValidateRefreshToken(req.RefreshToken)
	if err != nil {
		c.JSON(http.StatusUnauthorized, models.ErrorResponse{Error: "Invalid refresh token"})
		return
	}
	if req.All {
		user, err := h.mongoRepo.GetUserByEmail(ctx, email)
		if err == nil && user != nil {
			_, err = h.mongoRepo.UsersColl.UpdateOne(ctx, bson.M{"_id": user.ID}, bson.M{"$set": bson.M{"refresh_tokens": []models.RefreshToken{}}})
		}
	} else {
		_ = h.mongoRepo.RemoveRefreshToken(ctx, req.RefreshToken)
	}
	c.JSON(http.StatusOK, models.SuccessResponse{Message: "Logged out successfully"})
}

// ValidateTokenHandler - POST /auth/validate
func (h *AuthHandler) ValidateTokenHandler(c *gin.Context) {
	authHeader := c.GetHeader("Authorization")
	if authHeader == "" {
		c.JSON(http.StatusBadRequest, models.ErrorResponse{Error: "Authorization header is required"})
		return
	}

	// Извлекаем token из "Bearer <token>"
	if len(authHeader) <= 7 || authHeader[:7] != "Bearer " {
		c.JSON(http.StatusBadRequest, models.ErrorResponse{Error: "Invalid authorization header format"})
		return
	}

	accessToken := authHeader[7:]

	// Валидируем access token
	claims, err := h.jwtService.ValidateAccessToken(accessToken)
	if err != nil {
		c.JSON(http.StatusUnauthorized, models.ErrorResponse{Error: "Invalid access token"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"valid":       true,
		"permissions": claims.Permissions,
		"expires_at":  claims.ExpiresAt,
	})
}

// HealthCheckHandler - GET /health
func (h *AuthHandler) HealthCheckHandler(c *gin.Context) {
	ctx := c.Request.Context()

	if err := h.mongoRepo.Ping(ctx); err != nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"status": "unhealthy",
			"error":  "MongoDB connection failed",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"status":    "healthy",
		"timestamp": time.Now().Unix(),
		"service":   "auth-service",
	})
}
