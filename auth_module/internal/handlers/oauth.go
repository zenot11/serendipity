package handlers

import (
	"auth/internal/database/repository"
	"auth/internal/database/storage"
	"auth/internal/pkg/jwt"
	"auth/internal/pkg/oauth"
	"auth/internal/services"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
)

type OAuthHandler struct {
	jwtService     *jwt.JWTService
	oauthService   *oauth.OAuthService
	loginStateMgr  *repository.LoginStateManager
	mongoRepo      *storage.MongoRepository
	permissionsSvc *services.PermissionsService
}

func NewOAuthHandler(
	jwtService *jwt.JWTService,
	oauthService *oauth.OAuthService,
	mongoRepo *storage.MongoRepository,
	loginStateMgr *repository.LoginStateManager,
	permissionsSvc *services.PermissionsService,
) *OAuthHandler {
	return &OAuthHandler{
		jwtService:     jwtService,
		oauthService:   oauthService,
		mongoRepo:      mongoRepo,
		loginStateMgr:  loginStateMgr,
		permissionsSvc: permissionsSvc,
	}
}

// GitHubCallbackHandler - GET /auth/github/callback
func (h *OAuthHandler) GitHubCallbackHandler(c *gin.Context) {
	h.handleOAuthCallback(c, "github")
}

// YandexCallbackHandler - GET /auth/yandex/callback
func (h *OAuthHandler) YandexCallbackHandler(c *gin.Context) {
	h.handleOAuthCallback(c, "yandex")
}

// handleOAuthCallback обрабатывает OAuth callback
func (h *OAuthHandler) handleOAuthCallback(c *gin.Context, provider string) {
	code := c.Query("code")
	state := c.Query("state")
	errorParam := c.Query("error")

	// Если пользователь отказался
	if errorParam != "" || code == "" || state == "" {
		h.loginStateMgr.UpdateLoginState(state, "denied", "", "")
		c.JSON(http.StatusBadRequest, gin.H{
			"status": "denied",
			"error":  "oauth cancelled or invalid request",
		})
		return
	}

	// Обмениваем код на информацию о пользователе
	ctx := c.Request.Context()
	oauthUser, err := h.oauthService.ExchangeCode(ctx, provider, code)
	if err != nil || oauthUser == nil {
		h.loginStateMgr.UpdateLoginState(state, "denied", "", "")
		c.JSON(http.StatusUnauthorized, gin.H{
			"status": "denied",
			"error":  "oauth exchange failed",
		})
		return
	}

	// Проверяем или создаем пользователя
	user, err := h.mongoRepo.GetUserByEmail(ctx, oauthUser.Email)
	if err != nil {
		h.loginStateMgr.UpdateLoginState(state, "denied", "", "")
		c.JSON(http.StatusInternalServerError, gin.H{
			"status": "denied",
			"error":  "database error",
		})
		return
	}

	if user == nil {
		// Создаем нового пользователя
		user, err = h.mongoRepo.CreateUser(ctx, oauthUser.Email, oauthUser.DisplayName)
		if err != nil {
			h.loginStateMgr.UpdateLoginState(state, "denied", "", "")
			c.JSON(http.StatusInternalServerError, gin.H{
				"status": "denied",
				"error":  "failed to create user",
			})
			return
		}
	}

	// Проверяем блокировку
	if user.IsBlocked {
		h.loginStateMgr.UpdateLoginState(state, "denied", "", "")
		c.JSON(http.StatusForbidden, gin.H{
			"status": "denied",
			"error":  "user is blocked",
		})
		return
	}

	// Получаем разрешения
	perms := h.permissionsSvc.GetUserPermissions(user)

	// Генерируем токены
	tokenPair, err := h.jwtService.GenerateTokenPair(user, perms)
	if err != nil {
		h.loginStateMgr.UpdateLoginState(state, "denied", "", "")
		c.JSON(http.StatusInternalServerError, gin.H{
			"status": "denied",
			"error":  "token generation failed",
		})
		return
	}

	// Сохраняем refresh token
	refreshExpiry := time.Now().Add(7 * 24 * time.Hour)
	err = h.mongoRepo.AddRefreshToken(ctx, user.ID.Hex(), tokenPair.RefreshToken, refreshExpiry)
	if err != nil {
		h.loginStateMgr.UpdateLoginState(state, "denied", "", "")
		c.JSON(http.StatusInternalServerError, gin.H{
			"status": "denied",
			"error":  "failed to save refresh token",
		})
		return
	}
	// Обновляем статус входа
	h.loginStateMgr.UpdateLoginState(state, "granted", tokenPair.AccessToken, tokenPair.RefreshToken)

	// Успех
	c.JSON(http.StatusOK, gin.H{
		"status": "granted",
	})
}
